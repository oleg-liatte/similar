#include <iostream>
#include <sstream>
#include <fstream>
#include <string>
#include <stdexcept>
#include <unordered_map>
#include <thread>

#include <getopt.h>

#include "spanhash.hpp"
#include "directory.hpp"
#include "SHA1.h"
#include "progress.hpp"


namespace
{
	
	struct Match
	{
		Match():
			destination(),
			similarity(0.0f)
		{
		}

		template<typename Dest>
		Match(Dest&& dest, float sim):
			destination(std::forward<Dest>(dest)),
			similarity(sim)
		{
		}

		Match(const Match& that):
			destination(that.destination),
			similarity(that.similarity)
		{
		}
		
		Match(Match&& that):
			destination(std::move(that.destination)),
			similarity(that.similarity)
		{
		}
		
		Match& operator=(const Match& that)
		{
			destination = that.destination;
			similarity = that.similarity;
			return *this;
		}
		
		Match& operator=(Match&& that)
		{
			destination = std::move(that.destination);
			similarity = that.similarity;
			return *this;
		}
		
		std::string destination;
		float similarity;
	};
	
	void showHelp()
	{
		std::cerr <<
"Synopsis: similar [options] [--] <source> [<destination>...]\n"
"\n"
"Compare source with destination(s) and calculate similarity indices\n"
"\n"
"If source and/or destination is directory then this directory is scanned\n"
"recursively and all files inside are considered as source or destination\n"
"respectively.\n"
"\n"
"Similarity index is real number from 0 (files are completely different) to\n"
"1 (files are exactly similar).\n"
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
"    Dump output to a given file instead of stdout. In this case stdout is used\n";
"    to display a progress.\n";
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
	
	class SHA1Digest
	{
	public:
		SHA1Digest()
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
		
		bool operator==(const SHA1Digest& that) const
		{
			return memcmp(data_, that.data_, sizeof(data_)) == 0;
		}
		
		bool operator!=(const SHA1Digest& that) const
		{
			return !(*this == that);
		}

	private:
		UINT_8 data_[20];
		
	};
	
	typedef std::unordered_multimap<SHA1Digest, std::string> FileHashes;

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
	struct hash<SHA1Digest>
	{
		size_t operator()(const SHA1Digest& v) const
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
	
	while(true)
	{
		int c = getopt_long(argc, argv, "m:ao:", long_options, nullptr);
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
	
	// list files

	if(showProgress)
	{
		std::cout << step.step("Listing files...") << std::flush;
	}

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

		Directory dir(argv[i]);
		for(auto f: dir)
		{
			CSHA1 sha1;
			sha1.HashFile(f.c_str());
			sha1.Final();
			
			SHA1Digest digest;
			sha1.GetHash(digest.data());
			
			fh->emplace(digest, std::move(f));
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
	std::unordered_map<std::string, Match> matches;

	{
		if(showProgress)
		{
			progress.setPrefix(step.step("Searching exact matches: "));
			progress.setPostfix("%");
			progress.setCurrent(0.0f);
			progress.setTotal(static_cast<float>(source.size()) * destination->size());
			progress.update();
		}

		size_t matchesCount = 0;
		
		size_t srcIndex = 0;
		for(const auto& src: source)
		{
			size_t dstIndex = 0;
			for(const auto& dst: *destination)
			{
				if(src.first == dst.first && src.second != dst.second)
				{
					if(all)
					{
						out << "1|" << src.second << "|" << dst.second << std::endl;
						matchesCount += 1;
					}
					else
					{
						auto match = matches.emplace(src.second, Match(dst.second, 1.0f));
						if(match.second)
						{
							matchesCount += 1;
						}
					}
				}

				dstIndex += 1;
				if(showProgress)
				{
					progress.setCurrent(static_cast<float>(destination->size()) * srcIndex + dstIndex);
					progress.update();
				}
			}

			srcIndex += 1;
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
		for(const auto& src: source)
		{
			const SpanHash source(src.second.c_str());
			
			size_t dstIndex = 0;
			for(const auto& dst: *destination)
			{
				if(src.first != dst.first && src.second != dst.second)
				{
					const SpanHash destination(dst.second.c_str());

					// exact matches were found before so these files can't be exactly the same
					auto similarity = source.compare(destination) * 0.99f;
					
					if(similarity >= minSimilarity)
					{
						if(all)
						{
							out << similarity << "|" << src.second << "|" << dst.second << std::endl;
							matchesCount += 1;
						}
						else
						{
							Match m(dst.second, similarity);
							auto match = matches.emplace(src.second, m);
							if(match.second)
							{
								matchesCount += 1;
							}
							else if(similarity > match.first->second.similarity)
							{
								// there's existing match with lower similarity, replace it
								match.first->second = m;
							}
						}
					}
				}

				dstIndex += 1;
				if(showProgress)
				{
					progress.setCurrent(static_cast<float>(destination->size()) * srcIndex + dstIndex);
					progress.update();
				}
			}

			srcIndex += 1;
		}

		if(showProgress)
		{
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
			progress.setTotal(static_cast<float>(matches.size()));
			progress.update();
		}
		
		size_t matchIndex = 0;
		for(const auto& match: matches)
		{
			out << match.second.similarity << "|" << match.first << "|" << match.second.destination << std::endl;
			matchIndex += 1;
			if(showProgress)
			{
				progress.setCurrent(matchIndex);
				progress.update();
			}
		}

		if(showProgress)
		{
			progress.flush();
			std::cout << std::endl;
		}
	}
	
	return 0;
}

