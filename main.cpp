#include <iostream>
#include <algorithm>
#include <boost/iostreams/device/mapped_file.hpp>
#include <fstream>
#include <map>
#include <string>
#include <set>
#include <sstream>

#include <boost/filesystem.hpp>
#include <boost/integer.hpp>
#include <boost/crc.hpp>
#include <boost/iostreams/device/file.hpp>
#include <boost/iostreams/stream.hpp>
#include <boost/algorithm/string/join.hpp>

namespace io = boost::iostreams;
namespace fs = boost::filesystem;
///////////////////////////////////////////////////////////////////////////
// DATA DEF
///////////////////////////////////////////////////////////////////////////
// max amount of byte we read from the file to compute the CRC.
std::streamsize const  buffer_size = 2048;

struct file_cmp_options_t {
	file_cmp_options_t() : file_name(true), file_data(false) {}
	bool file_name;
	bool file_data;
};

typedef std::multimap<boost::int32_t, fs::path> crc32_to_path_t;
typedef std::map<fs::path, std::vector<fs::path>> equals_t;
typedef std::map<std::string, std::set<std::string> > reduce_t;

struct work_package_t {
	file_cmp_options_t 		file_cmp_options;
	crc32_to_path_t 			crc32_to_path;
	equals_t							equals;
	reduce_t							reduce;
};

struct crc_failed_t : public boost::exception, public std::exception {
  const char *what() const noexcept { return "CRC computation failed"; }
};


///////////////////////////////////////////////////////////////////////////
// CODE
///////////////////////////////////////////////////////////////////////////

// depending on the options, CRC32 will be either computed using the file name, or by using the data withing the file.
boost::int32_t file_crc32(const file_cmp_options_t& options, const fs::path& file_path) {
	boost::crc_32_type  result;
	if (options.file_name && !options.file_data) {
		std::string fileName = file_path.filename().string();
		result.process_bytes(fileName.c_str(), fileName.size());
	} else {
		std::ifstream	input(file_path.c_str(), std::ios::binary);
		if (input) {
			char  buffer[ buffer_size ];
	    input.read( buffer, buffer_size );
	    result.process_bytes( buffer, input.gcount() );
		} else {
			throw crc_failed_t();
		}
	}
	return result.checksum();
}


// tells if two file are equal, by, depending on the options, comparing the file names and/or comparing the raw data.
bool file_compare(const file_cmp_options_t& options, const fs::path& left_path, const fs::path& right_path) {
	bool is_equal = true;
	if (options.file_name) {
		is_equal = left_path.filename() == right_path.filename();
	}
	if (options.file_data) {
		try {
			io::mapped_file_source left(left_path);
			io::mapped_file_source right(right_path);
			// visual warning here is not valid.
			is_equal &= (left.size() == right.size())
				&& std::equal(left.data(), left.data() + left.size(), right.data());
		} catch (boost::exception& ) {
			return false;
		}
	}
	return is_equal;
}

// find out if the file is duplicated and store the data as needed.
void check_equals(work_package_t &work_package, const fs::path& path) {
	auto 	crc32 = file_crc32(work_package.file_cmp_options, path);
	auto 	range = work_package.crc32_to_path.equal_range(crc32);
	bool	need_to_add = true;

	for (auto it = range.first; it != range.second; ++it) {
		auto cmp_path = it->second;
		if (file_compare(work_package.file_cmp_options, cmp_path, path)) {
			need_to_add = false;
			auto equal_it = work_package.equals.find(cmp_path);
			if (equal_it == work_package.equals.end()) {
				work_package.equals[cmp_path].push_back(cmp_path);
			}
			work_package.equals[cmp_path].push_back(path);
		}
	}
	if (need_to_add) {
		work_package.crc32_to_path.insert(std::make_pair(crc32, path));
	}
}

void find_duplicate(const fs::path& p, work_package_t&	work_package) {
	fs::recursive_directory_iterator it(p);
	fs::recursive_directory_iterator end;
	while (it != end) {
		auto entry = *it++;
		auto entryPath = entry.path();
		if (fs::is_regular_file(entryPath)) {
			try {
				// find if we have some files that have the same CRC
				check_equals(work_package, entryPath);
			} catch(crc_failed_t&) {
				// means we were not able to read the file, skip it ( might want to stderr )
			}
		}
	}
}

void reduce_result(work_package_t& work_package) {
	for (auto it = work_package.equals.begin(); it != work_package.equals.end(); ++it) {
		auto 										paths = it->second;
		std::set<std::string>  	names;
		std::set<std::string> 	directories;

		for (auto it_path = paths.begin(); it_path != paths.end(); ++it_path) {
			const fs::path& path = (*it_path);
			directories.insert(fs::canonical(path.parent_path()).string());
			names.insert(path.filename().string());
		}

		auto joinedNames = "\t" + boost::algorithm::join(names, " == ");
		auto joinedDirectories = boost::algorithm::join(directories, "\n");
		work_package.reduce[joinedDirectories].insert(joinedNames);
	}
}

void print_reduced_result(work_package_t& work_package) {
	for (auto it = work_package.reduce.begin(); it != work_package.reduce.end(); ++it) {
		std::cout << it->first << std::endl;
		std::cout << boost::algorithm::join(it->second, "\n") << std::endl;
	}
}

int main(int argc, char* argv[]) {
	if (argc > 1) {
		work_package_t	work_package;
		for (int i = 1; i < argc; ++i) {
			if (strcmp("-data", argv[i]) == 0)
				work_package.file_cmp_options.file_data = false;
			else if (strcmp("-name", argv[i]) == 0)
				work_package.file_cmp_options.file_name = false;
			else if (strcmp("+data", argv[i]) == 0)
				work_package.file_cmp_options.file_data = true;
			else if (strcmp("+name", argv[i]) == 0)
				work_package.file_cmp_options.file_name = true;
			else {
				fs::path p(argv[i]);
				if (fs::exists(p)) {
					find_duplicate(p, work_package);
				}
			}
		}
		reduce_result(work_package);
		print_reduced_result(work_package);
	}

  return 0;
}
