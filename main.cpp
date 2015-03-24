#include <iostream>
#include <sstream>
#include <fstream>
#include <string>
#include <stdexcept>
#include <vector>
#include <unordered_set>
#include <algorithm>
#include <thread>

#include <getopt.h>
#include <assert.h>
#include <string.h>

#include "spanhash.hpp"
#include "directory_walker.hpp"
#include "SHA1.h"
#include "progress.hpp"
#include "async_manager.hpp"


namespace
{

	void showHelp()
	{
		std::cerr <<
"Synopsis: similar [options] [<source> [<destination>...]]\n"
"\n"
"Compare source with destination(s) and calculate similarity indices.\n"
"\n"
"If source and/or destination is directory then this directory is scanned\n"
"recursively and all files inside are considered as source or destination\n"
"respectively.\n"
"\n"
"Similarity index is real number from 0 (files are completely different) to\n"
"1 (files are exactly the same).\n"
"\n"
"By default only best matches are displayed. To compare each source with each\n"
"destination use --all option.\n"
"\n"
"Options:\n"
"-s, --source <path>\n"
"    Add source file or directory.\n"
"-d, --destination <path>\n"
"    Add destination file or directory.\n"
"-S, --source-list [<prefix>:]<path>\n"
"    Add sources from the given file that contains a list of paths (one path\n"
"    per line). Optional prefix is prepended to each path.\n"
"-D, --destination-list [<prefix>:]<path>\n"
"    Add destinations from the given file that contains a list of paths (one path\n"
"    per line). Optional prefix is prepended to each path.\n"
"-l, --follow-symlinks\n"
"    Follow symlinks instead of treating them as links.\n"
"-L, --dont-follow-symlinks\n"
"    Don't follow symlinks and treat them as links. This is default.\n"
"-m, --min-similarity <index>\n"
"    Minimum similarity index for files that can be considered similar. Valid\n"
"    value range is [0 .. 1]. Default is 0.5.\n"
"-a, --all\n"
"    Display all sources with all destinations comparisons.\n"
"-o, --out <file>\n"
"    Dump output to a given file instead of stdout. In this case stdout is used\n"
"    to display a progress.\n"
"-t, --text\n"
"    Check similarity only for text files. Binary files are checked only for\n"
"    exact match.\n"
"-h, --help\n"
"    Show this help and exit.\n";
	}

	template<typename T>
	bool lexicalCast(const char* str, T& val)
	{
		std::stringstream ss(str);
		ss >> val;
		return ss.rdstate() == std::ios_base::eofbit;
	}

	bool fileIsBinary(const char* fileName)
	{
		std::ifstream stream(fileName, std::ios_base::in | std::ios_base::binary);
		if(!stream.is_open())
		{
			throw std::runtime_error(std::string("failed to open file: '") + fileName + "'");
		}

		// read up to 1024 bytes
		const size_t MAX_READ = 1024;
		bool result = false;
		for(size_t i = 0; i != MAX_READ; ++i)
		{
			int ch = stream.get();
			if(ch == EOF)
			{
				break;
			}

			if(!std::isprint(ch) && !std::isspace(ch))
			{
				result = true;
				break;
			}
		}

		return result;
	}

	class FileDigest
	{
	public:
		FileDigest()
		{
			memset(data_, 0, sizeof(data_));
		}

		FileDigest(const FileDigest& that)
		{
			memcpy(data_, that.data_, sizeof(data_));
		}

		FileDigest& operator=(const FileDigest& that)
		{
			memcpy(data_, that.data_, sizeof(data_));
			return *this;
		}

		UINT_8* data()
		{
			return data_;
		}

		const UINT_8* data() const
		{
			return data_;
		}

		size_t hash() const
		{
			return *reinterpret_cast<const size_t*>(data_);
		}

		bool operator==(const FileDigest& that) const
		{
			return memcmp(data_, that.data_, sizeof(data_)) == 0;
		}

		bool operator!=(const FileDigest& that) const
		{
			return !(*this == that);
		}

	private:
		UINT_8 data_[20];

	};

	class FileInfo
	{
	public:
		struct Match
		{
			Match(FileInfo* fileInfo, float similarity):
				fileInfo(fileInfo),
				similarity(similarity)
			{
			}

			FileInfo* fileInfo;
			float similarity;
		};

		FileInfo(std::string&& name, size_t size):
			name_(std::move(name)),
			size_(size),
			binary_(false),
			digest_(),
			spanHash_(),
			spanHashRefs_(0),
			matches_()
		{
		}

		FileInfo(FileInfo&& that):
			name_(std::move(that.name_)),
			size_(that.size_),
			binary_(that.binary_),
			digest_(std::move(that.digest_)),
			spanHash_(std::move(that.spanHash_)),
			spanHashRefs_(that.spanHashRefs_),
			matches_(std::move(that.matches_))
		{
			that.spanHashRefs_ = 0;
		}

		FileInfo& operator=(FileInfo&& that)
		{
			name_ = std::move(that.name_);
			size_ = that.size_;
			binary_ = that.binary_;
			digest_ = std::move(that.digest_);
			spanHash_ = std::move(that.spanHash_);
			spanHashRefs_ = that.spanHashRefs_;
			that.spanHashRefs_ = 0;
			matches_ = std::move(that.matches_);
		}

		const std::string& name() const
		{
			return name_;
		}

		size_t size() const
		{
			return size_;
		}

		bool isBinary() const
		{
			return binary_;
		}

		const FileDigest& digest() const
		{
			return digest_;
		}

		const SpanHash& spanHash() const
		{
			return spanHash_;
		}

		void acquireSpanHash()
		{
			if(spanHashRefs_ == -1)
			{
				// counter overflow, don't increment it further
				return;
			}

			spanHashRefs_ += 1;

			if(spanHashRefs_ == 1)
			{
				spanHash_.init(name_.c_str(), binary_);
			}
		}

		void releaseSpanHash()
		{
			if(spanHashRefs_ == -1)
			{
				// counter overflow, don't ever release
				return;
			}

			if(spanHashRefs_ > 0)
			{
				spanHashRefs_ -= 1;
			}

			if(spanHashRefs_ == 0)
			{
				spanHash_.clear();
			}
		}

		bool read()
		{
			// check if file is binary
			binary_ = fileIsBinary(name_.c_str());

			// calculate file digest
			CSHA1 sha1;
			if(!sha1.HashFile(name_.c_str()))
			{
				std::cerr << "ERROR: failed to read file: '" << name_ << "'" << std::endl;
				return false;
			}
			sha1.Final();

			sha1.GetHash(digest_.data());

			return true;
		}

		bool addMatch(FileInfo* that, float similarity)
		{
			auto matchSortPredicate = [](const Match& l, const Match& r)
			{
				return l.similarity > r.similarity;
			};

			// check if that file is already here
			auto existing = std::find_if(matches_.begin(), matches_.end(), [that](const Match& match)
			{
				return match.fileInfo == that;
			});

			if(existing != matches_.end())
			{
				return false;
			}

			// find appropriate insertion position
			const Match match(that, similarity);
			auto pos = std::upper_bound(matches_.begin(), matches_.end(), match, matchSortPredicate);

			// insert match
			matches_.insert(pos, match);

			that->addMatch(this, similarity);

			return matches_.size() == 1; // we have added the first match
		}

		bool hasMatch() const
		{
			return !matches_.empty();
		}

		bool hasMatch(float similarity) const
		{
			if(!matches_.empty())
			{
				return matches_.front().similarity >= similarity;
			}
			else
			{
				return false;
			}
		}

		bool takeMatch(float& similarity, FileInfo*& source, FileInfo*& destination)
		{
			if(matches_.empty())
			{
				return false;
			}

			source = this;

			// find the first mutual match
			for(size_t recursionCounter = 1000; recursionCounter != 0; --recursionCounter)
			{
				assert(!source->matches_.empty());
				auto& sourceMatch = source->matches_.front();

				destination = sourceMatch.fileInfo;
				assert(!destination->matches_.empty());
				auto& destinationMatch = destination->matches_.front();

				if(destinationMatch.fileInfo == source)
				{
					similarity = sourceMatch.similarity;

					source->clearMatches();
					destination->clearMatches();

					return true;
				}

				source = destinationMatch.fileInfo;
			}

			std::cerr << "ERROR: similarity chain seems to contain loop, this shouldn't have happen";
			return false;
		}

	private:
		std::string name_;
		size_t size_;
		bool binary_;
		FileDigest digest_;
		SpanHash spanHash_;
		size_t spanHashRefs_;
		std::vector<Match> matches_;

		void removeMatch(FileInfo* that)
		{
			auto match = std::find_if(matches_.begin(), matches_.end(), [that](const Match& match)
			{
				return match.fileInfo == that;
			});

			if(match == matches_.end())
			{
				return;
			}

			matches_.erase(match);
		}

		void clearMatches()
		{
			std::vector<Match> old;
			matches_.swap(old);

			for(const auto& match: old)
			{
				match.fileInfo->removeMatch(this);
			}
		}

	};

	struct DigestIndexPred
	{
		bool operator()(FileInfo* lhs, FileInfo* rhs) const
		{
			return lhs->digest() == rhs->digest();
		}

		size_t operator()(FileInfo* v) const
		{
			return v->digest().hash();
		}
	};

	typedef std::unordered_multiset<FileInfo*, DigestIndexPred, DigestIndexPred> DigestIndex;

	template<typename T>
	const char* plural(T count, const char* postfix = "s")
	{
		if(count != 1)
		{
			return postfix;
		}
		else
		{
			return "";
		}
	}

	class Step
	{
	public:
		explicit Step(unsigned total):
			current_(0),
			total_(total)
		{
		}

		std::string step(const char* title)
		{
			current_ += 1;

			std::stringstream ss;
			ss << "[" << current_ << "/" << total_ << "] " << title;

			return ss.str();
		}

	private:
		unsigned current_;
		unsigned total_;

	};

	typedef std::vector<FileInfo> FileList;

	void addPath(FileList& list, const char* path, bool followSymlinks)
	{
		//std::cerr << "adding " << path << std::endl;
		const Directory::Stat stat(path, followSymlinks);
		switch(stat.fileType)
		{
		case Directory::Stat::Directory:
			for(auto f: DirectoryWalker(path, followSymlinks))
			{
				if(f.second.fileType == Directory::Stat::Regular)
				{
					//std::cerr << "found " << f.first << std::endl;
					list.emplace_back(std::move(f.first), f.second.size);
				}
			}
			break;

		case Directory::Stat::Regular:
			//std::cerr << "file " << path << std::endl;
			list.emplace_back(path, stat.size);
			break;

		}
	}

	bool xgetline(std::istream& stream, std::string& out)
	{
		bool nonEmpty = false;
		for(int c = stream.get(); c != EOF; c = stream.get())
		{
			if(c == '\r' || c == '\n')
			{
				break;
			}

			out.push_back(c);
			nonEmpty = true;
		}

		return nonEmpty;
	}

	void addListFile(FileList& list, const char* path, bool followSymlinks)
	{
		const char* colon = strchr(path, ':');
		const char* p = colon ? colon + 1 : path;

		std::ifstream ifs(p, std::ios_base::in | std::ios_base::binary);
		if(!ifs.good())
		{
			std::cerr << "ERROR: failed to open file: '" << p << "'" << std::endl;
			return;
		}

		std::string f;
		do
		{
			if(colon)
			{
				f.assign(path, colon);
			}
			else
			{
				f.clear();
			}

			if(!xgetline(ifs, f))
			{
				continue;
			}

			addPath(list, f.c_str(), followSymlinks);
		}
		while(ifs.good());
	}

}


int main(int argc, char** argv)
{
	// parse options

	static const char short_options[] = "s:d:S:D:lLm:ao:th";
	static const option long_options[] =
	{
		{
			.name = "source",
			.has_arg = required_argument,
			.flag = nullptr,
			.val = 's'
		},
		{
			.name = "destination",
			.has_arg = required_argument,
			.flag = nullptr,
			.val = 'd'
		},
		{
			.name = "source-list",
			.has_arg = required_argument,
			.flag = nullptr,
			.val = 'S'
		},
		{
			.name = "destination-list",
			.has_arg = required_argument,
			.flag = nullptr,
			.val = 'D'
		},
		{
			.name = "follow-symlinks",
			.has_arg = no_argument,
			.flag = nullptr,
			.val = 'l'
		},
		{
			.name = "dont-follow-symlinks",
			.has_arg = no_argument,
			.flag = nullptr,
			.val = 'L'
		},
		{
			.name = "min-similarity",
			.has_arg = required_argument,
			.flag = nullptr,
			.val = 'm'
		},
		{
			.name = "all",
			.has_arg = no_argument,
			.flag = nullptr,
			.val = 'a'
		},
		{
			.name = "out",
			.has_arg = required_argument,
			.flag = nullptr,
			.val = 'o'
		},
		{
			.name = "text",
			.has_arg = no_argument,
			.flag = nullptr,
			.val = 't'
		},
		{
			.name = "help",
			.has_arg = no_argument,
			.flag = nullptr,
			.val = 'h'
		},
		{
			.name = nullptr,
			.has_arg = 0,
			.flag = nullptr,
			.val = 0
		}
	};

	float minSimilarity = 0.5f;
	bool all = false;
	bool exactOnly = false;
	std::string outFile;
	bool textOnly = false;

	while(true)
	{
		int c = getopt_long(argc, argv, short_options, long_options, nullptr);
		if(c < 0)
		{
			break;
		}

		switch(c)
		{
		case 's':
		case 'd':
		case 'S':
		case 'D':
		case 'l':
		case 'L':
			// nop
			break;

		case 'm':
			if(!lexicalCast(optarg, minSimilarity) || minSimilarity < 0.0f || minSimilarity > 1.0f)
			{
				std::cerr << "ERROR: invalid min-similarity value: " << optarg << std::endl;
				showHelp();
				return 1;
			}
			exactOnly = (minSimilarity >= 1.0f);
			break;

		case 'a':
			all = true;
			break;

		case 'o':
			outFile = optarg;
			break;

		case 't':
			textOnly = true;
			break;

		case 'h':
			showHelp();
			return 0;

		default:
			showHelp();
			return 1;
		}
	}

	std::ofstream outStream;
	bool showProgress = !outFile.empty();
	if(showProgress)
	{
		outStream.open(outFile, std::ios_base::out | std::ios_base::trunc);
		if(!outStream.is_open())
		{
			std::cerr << "ERROR: failed to open file: " << outFile << std::endl;
		}
	}

	std::ostream& out = outFile.empty() ? std::cout : outStream;

	unsigned totalSteps = 5;
	if(all)
	{
		totalSteps -= 1;
	}

	if(exactOnly)
	{
		totalSteps -= 1;
	}

	Step step(totalSteps);
	Progress progress;

	// 1. List files

	if(showProgress)
	{
		std::cout << step.step("Listing files...") << std::flush;
	}

	FileList source;
	FileList destination_storage;
	bool haveDestination = false;
	bool followSymlinks = false;

	optind = 1; // restart options parser
	while(true)
	{
		int c = getopt_long(argc, argv, short_options, long_options, nullptr);
		if(c < 0)
		{
			break;
		}

		switch(c)
		{
		case 's':
			addPath(source, optarg, followSymlinks);
			break;

		case 'd':
			addPath(destination_storage, optarg, followSymlinks);
			haveDestination = true;
			break;

		case 'S':
			addListFile(source, optarg, followSymlinks);
			break;

		case 'D':
			addListFile(destination_storage, optarg, followSymlinks);
			haveDestination = true;
			break;

		case 'l':
			followSymlinks = true;
			break;

		case 'L':
			followSymlinks = false;
			break;

		}
	}

	haveDestination |= argc - optind > 1;

	for(int i = optind; i < argc; ++i)
	{
		FileList& list = (i > optind) ? destination_storage : source;
		addPath(list, argv[i], followSymlinks);
	}

	FileList& destination = haveDestination ? destination_storage : source;

	if(showProgress)
	{
		std::cout << " "
			<< source.size() << " source" << plural(source.size()) << ", "
			<< destination.size() << " destination" << plural(destination.size()) << std::endl;
	}

	if(source.empty())
	{
		std::cerr << "ERROR: source file list is empty" << std::endl;
		return 1;
	}

	if(destination.empty())
	{
		std::cerr << "ERROR: destination file list is empty" << std::endl;
		return 1;
	}

	// 2. Hash files

	DigestIndex destinationDigestIndex;

	{
		if(showProgress)
		{
			progress.setPrefix(step.step("Hashing files: "));
			progress.setPostfix("%");
			progress.setCurrent(0.0f);
			progress.setTotal(source.size() + destination_storage.size());
			progress.update();
		}

		size_t fileIndex = 0;

		destinationDigestIndex.reserve(destination.size());

		for(int i = 0; i < 2; ++i)
		{
			const bool dest = (i > 0);
			FileList& list = dest ? destination_storage : source;
			for(auto& fi: list)
			{
				bool ok = fi.read();
				if(&list == &destination && ok)
				{
					// add file to digest index
					destinationDigestIndex.insert(&fi);
				}

				if(showProgress)
				{
					fileIndex += 1;
					progress.setCurrent(fileIndex);
					progress.update();
				}
			}
		}

		if(showProgress)
		{
			progress.setCurrent(progress.total());
			progress.flush();
			std::cout << std::endl;
		}
	}

	// 3. Search exact matches

	{
		if(showProgress)
		{
			progress.setPrefix(step.step("Searching exact matches: "));
			progress.setPostfix("%");
			progress.setCurrent(0.0f);
			progress.setTotal(source.size());
			progress.update();
		}

		size_t matchesCount = 0;

		for(size_t srcIndex = 0; srcIndex != source.size(); ++srcIndex)
		{
			auto& src = source[srcIndex];

			if(!all && src.hasMatch())
			{
				// skip already matched file
				continue;
			}

			auto dstRange = destinationDigestIndex.equal_range(&src);
			for(auto dstIt = dstRange.first; dstIt != dstRange.second; ++dstIt)
			{
				auto& dst = **dstIt;

				if(src.name() == dst.name())
				{
					// don't compare the file with itself
					continue;
				}

				if(all)
				{
					out << "1|" << src.name() << "|" << dst.name() << std::endl;
					matchesCount += 1;
					continue;
				}

				if(dst.hasMatch())
				{
					// skip already processed file
					continue;
				}

				src.addMatch(&dst, 1.0f);
				matchesCount += 1;
				break;
			}

			if(showProgress)
			{
				progress.setCurrent(srcIndex + 1);
				progress.update();
			}
		}

		if(showProgress)
		{
			progress.setCurrent(progress.total());
			progress.flush();
			std::cout << ", found " << matchesCount << " exact match" << plural(matchesCount, "es") << std::endl;
		}
	}

	if(!exactOnly)
	{
		// 4. Find similar files

		if(showProgress)
		{
			progress.setPrefix(step.step("Searching similar files: "));
			progress.setPostfix("%");
			progress.setCurrent(0.0f);
			progress.setTotal(static_cast<float>(source.size()) * destination.size());
			progress.update();
		}

		size_t matchesCount = 0;

		for(size_t srcIndex = 0; srcIndex != source.size(); ++srcIndex)
		{
			auto& src = source[srcIndex];

			if(textOnly && src.isBinary())
			{
				// skip binaries
				continue;
			}

			if(!all && src.hasMatch(1.0f))
			{
				// skip files with exact match
				continue;
			}

			src.acquireSpanHash(); // extra reference to avoid races inside loop

			for(size_t dstIndex = 0; dstIndex != destination.size(); ++dstIndex)
			{
				auto& dst = destination[dstIndex];

				if(textOnly && dst.isBinary())
				{
					// skip binaries
					continue;
				}

				if(!all && dst.hasMatch(1.0f))
				{
					// skip files with exact match
					continue;
				}

				if(src.digest() == dst.digest())
				{
					// skip exact matches
					continue;
				}

				if(src.name() == dst.name())
				{
					// don't compare the file with itself
					continue;
				}

				// check file sizes
				const size_t minSize = std::min(src.size(), dst.size());
				const size_t maxSize = std::max(src.size(), dst.size());
				const float maxSimilarity = static_cast<float>(minSize) / maxSize * 2.0f; // take LF & CRLF equivalence into account
				if(maxSimilarity < minSimilarity)
				{
					// maximum possible similarity is below limit
					continue;
				}

				src.acquireSpanHash();
				dst.acquireSpanHash();
				if(!src.spanHash().isValid() || !dst.spanHash().isValid())
				{
					continue;
				}

				AsyncManager::async([&, srcIndex, dstIndex]
				{
					// exact matches were found before so these files can't be exactly the same
					auto similarity = src.spanHash().compare(dst.spanHash()) * 0.99f;

					AsyncManager::sync([&, srcIndex, dstIndex, similarity]
					{
						src.releaseSpanHash();
						// dst.releaseSpanHash(); // don't release dst to avoid its re-read by the next src

						if(similarity >= minSimilarity)
						{
							if(all)
							{
								out << similarity << "|" << src.name() << "|" << dst.name() << std::endl;
								matchesCount += 1;
							}
							else
							{
								if(src.addMatch(&dst, similarity))
								{
									matchesCount += 1;
								}
							}
						}

						if(showProgress)
						{
							auto cur = std::max(progress.current(), static_cast<float>(destination.size()) * srcIndex + dstIndex + 1);
							progress.setCurrent(cur);
							progress.update();
						}
					});
				});

				AsyncManager::tick();
			}

			src.releaseSpanHash(); // release extra reference
		}

		AsyncManager::joinAll();

		if(showProgress)
		{
			progress.setCurrent(progress.total());
			progress.flush();
			std::cout << ", found " << matchesCount << " similar file pair" << plural(matchesCount) << std::endl;
		}
	}

	if(!all)
	{
		// 5. Dump matches
		if(showProgress)
		{
			progress.setPrefix(step.step("Dumping matches: "));
			progress.setPostfix("%");
			progress.setCurrent(0.0f);
			progress.setTotal(static_cast<float>(source.size()));
			progress.update();
		}

		for(size_t srcIndex = 0; srcIndex != source.size(); ++srcIndex)
		{
			auto& src = source[srcIndex];

			float sim = 0.0f;
			FileInfo* s = nullptr;
			FileInfo* d = nullptr;
			while(src.takeMatch(sim, s, d))
			{
				out << sim << "|" << s->name() << "|" << d->name() << std::endl;

				if(showProgress)
				{
					progress.setCurrent(srcIndex + 1);
					progress.update();
				}
			}
		}

		if(showProgress)
		{
			progress.setCurrent(progress.total());
			progress.flush();
			std::cout << std::endl;
		}
	}

	return 0;
}

