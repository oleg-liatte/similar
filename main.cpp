#include <iostream>
#include <sstream>
#include <fstream>
#include <string>
#include <stdexcept>
#include <unordered_map>

#include <getopt.h>

#include "spanhash.hpp"
#include "directory.hpp"
#include "SHA1.h"
#include "progress.hpp"
#include "match_map.hpp"


namespace
{
	
	void showHelp()
	{
		std::cerr <<
"Synopsis: similar [options] [--] <source> [<destination>...]\n"
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
	
	class FileDigest
	{
	public:
		FileDigest()
		{
			memset(data_, 0, sizeof(data_));
		}
		
		UINT_8* data()
		{
			return data_;
		}
		
		const UINT_8* data() const
		{
			return data_;
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
	
	bool isBinary(const char* fileName)
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

	struct FileInfo
	{
		FileInfo(std::string&& name, size_t size, bool binary):
			name(std::move(name)),
			size(size),
			binary(binary),
			processed(false)
		{
		}

		FileInfo(FileInfo&& that):
			name(std::move(that.name)),
			size(that.size),
			binary(that.binary),
			processed(that.processed)
		{
		}
		
		FileInfo& operator=(FileInfo&& that)
		{
			name = std::move(that.name);
			size = that.size;
			binary = that.binary;
			processed = that.processed;
		}
		
		std::string name;
		size_t size;
		bool binary;
		bool processed;
	};
	
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

}


namespace std
{
	
	template<>
	struct hash<FileDigest>
	{
		size_t operator()(const FileDigest& v) const
		{
			return *reinterpret_cast<const size_t*>(v.data());
		}
	};
	
}


int main(int argc, char** argv)
{
	// parse options

	static const option long_options[] =
	{
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
	std::string outFile;
	bool textOnly = false;
	
	while(true)
	{
		int c = getopt_long(argc, argv, "m:ao:th", long_options, nullptr);
		if(c < 0)
		{
			break;
		}
		
		switch(c)
		{
		case 'm':
			if(!lexicalCast(optarg, minSimilarity) || minSimilarity < 0.0f || minSimilarity > 1.0f)
			{
				std::cerr << "invalid min-similarity value: " << optarg << std::endl;
				showHelp();
				return 1;
			}
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
	
	if(optind == argc)
	{
		showHelp();
		return 1;
	}
	
	std::ofstream outStream;
	bool showProgress = !outFile.empty();
	if(showProgress)
	{
		outStream.open(outFile, std::ios_base::out | std::ios_base::trunc);
		if(!outStream.is_open())
		{
			std::cerr << "failed to open file: " << outFile << std::endl;
		}
	}
	
	std::ostream& out = outFile.empty() ? std::cout : outStream;
	
	Step step(all ? 3 : 4);
	
	// read files

	if(showProgress)
	{
		std::cout << step.step("Reading files...") << std::flush;
	}

	typedef std::unordered_multimap<FileDigest, FileInfo> FileHashes;

	FileHashes source;
	FileHashes destination_storage;
	
	FileHashes* destination = nullptr;
	
	for(int i = optind; i < argc; ++i)
	{
		FileHashes* fh;
		if(i == optind)
		{
			fh = &source;
		}
		else
		{
			destination = &destination_storage;
			fh = destination;
		}

		for(auto f: Directory(argv[i]))
		{
			bool binary = isBinary(f.c_str());

			CSHA1 sha1;
			size_t fileSize = sha1.HashFile(f.c_str());
			if(fileSize == -1)
			{
				std::cerr << "failed to read file: '" << f << "'" << std::endl;
				return 1;
			}
			sha1.Final();
			
			FileDigest digest;
			sha1.GetHash(digest.data());
			
			fh->emplace(digest, FileInfo(std::move(f), fileSize, binary));
		}
	}
	
	if(!destination)
	{
		destination = &source;
	}
	
	if(showProgress)
	{
		std::cout << " "
			<< source.size() << " source" << plural(source.size()) << ", "
			<< destination->size() << " destination" << plural(destination->size()) << std::endl;
	}

	// search exact matches

	Progress progress;
	MatchMap matchMap;

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
		
		size_t srcIndex = 0;
		for(auto srcIt = source.begin(); srcIt != source.end(); ++srcIt)
		{
			auto dstRange = destination->equal_range(srcIt->first);
			for(auto dstIt = dstRange.first; dstIt != dstRange.second; ++dstIt)
			{
				if(srcIt->second.name == dstIt->second.name)
				{
					// don't compare the file with itself
					continue;
				}

				if(all)
				{
					out << "1|" << srcIt->second.name << "|" << dstIt->second.name << std::endl;
					matchesCount += 1;
					continue;
				}

				if( !srcIt->second.processed &&
					!dstIt->second.processed &&
					matchMap.add(srcIt->second.name, dstIt->second.name, 1.0f))
				{
					srcIt->second.processed = true;
					dstIt->second.processed = true;
					matchesCount += 1;
				}
			}
			
			if(showProgress)
			{
				srcIndex += 1;
				progress.setCurrent(srcIndex);
				progress.update();
			}
		}
		
		if(!all)
		{
			
		}
		
		if(showProgress)
		{
			progress.flush();
			std::cout << ", found " << matchesCount << " exact match" << plural(matchesCount, "es") << std::endl;
		}
	}
	
	// find similar files
	
	try
	{
		if(showProgress)
		{
			progress.setPrefix(step.step("Searching similar files: "));
			progress.setPostfix("%");
			progress.setCurrent(0.0f);
			progress.setTotal(static_cast<float>(source.size()) * destination->size());
			progress.update();
		}

		size_t matchesCount = 0;

		size_t srcIndex = 0;
		for(auto srcIt = source.begin(); srcIt != source.end(); ++srcIt, ++srcIndex)
		{
			if(textOnly && srcIt->second.binary)
			{
				// skip binaries
				continue;
			}
			
			if(!all && srcIt->second.processed)
			{
				// skip files with exact match
				continue;
			}
			
			const SpanHash sourceHash(srcIt->second.name.c_str(), srcIt->second.binary);
			
			size_t dstIndex = 0;
			for(auto dstIt = destination->begin(); dstIt != destination->end(); ++dstIt, ++dstIndex)
			{
				if(textOnly && dstIt->second.binary)
				{
					// skip binaries
					continue;
				}

				if(!all && dstIt->second.processed)
				{
					// skip files with exact match
					continue;
				}

				if(srcIt->first == dstIt->first)
				{
					// skip exact matches
					continue;
				}

				if(srcIt->second.name == dstIt->second.name)
				{
					// don't compare the file with itself
					continue;
				}

				// check file sizes
				const size_t minSize = std::min(srcIt->second.size, dstIt->second.size);
				const size_t maxSize = std::max(srcIt->second.size, dstIt->second.size);
				const float maxSimilarity = static_cast<float>(minSize) / maxSize * 2.0f; // take LF & CRLF equivalence into account
				if(maxSimilarity < minSimilarity)
				{
					// maximum possible similarity is below limit
					continue;
				}

				const SpanHash destinationHash(dstIt->second.name.c_str(), dstIt->second.binary);

				// exact matches were found before so these files can't be exactly the same
				auto similarity = sourceHash.compare(destinationHash) * 0.99f;
				
				if(similarity >= minSimilarity)
				{
					if(all)
					{
						out << similarity << "|" << srcIt->second.name << "|" << dstIt->second.name << std::endl;
						matchesCount += 1;
					}
					else
					{
						if(matchMap.add(srcIt->second.name, dstIt->second.name, similarity))
						{
							matchesCount += 1;
						}
					}
				}

				if(showProgress)
				{
					progress.setCurrent(static_cast<float>(destination->size()) * srcIndex + dstIndex);
					progress.update();
				}
			}
		}

		if(showProgress)
		{
			progress.setCurrent(progress.total());
			progress.flush();
			std::cout << ", found " << matchesCount << " similar file pair" << plural(matchesCount) << std::endl;
		}
	}
	catch(std::exception& e)
	{
		std::cerr << e.what() << std::endl;
		return 1;
	}
	
	if(!all)
	{
		if(showProgress)
		{
			progress.setPrefix(step.step("Dumping matches: "));
			progress.setPostfix("%");
			progress.setCurrent(0.0f);
			progress.setTotal(static_cast<float>(matchMap.size()));
			progress.update();
		}
		
		size_t matchIndex = 0;
		matchMap.for_each([&](const std::string& src, const std::string& dst, float similarity)
		{
			out << similarity << "|" << src << "|" << dst << std::endl;
			if(showProgress)
			{
				matchIndex += 1;
				progress.setCurrent(matchIndex);
				progress.update();
			}
		});

		if(showProgress)
		{
			progress.flush();
			std::cout << std::endl;
		}
	}
	
	return 0;
}

