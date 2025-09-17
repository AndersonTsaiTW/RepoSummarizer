#include <filesystem>
#include <system_error>
#include <sstream>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <algorithm>

namespace fs = std::filesystem;

static const char* kToolName = "repopac";
static const char* kVersion = "0.1.0";

static constexpr std::size_t kMaxBytes = 16 * 1024;

static void print_help() {
	std::cout << "Usage: " << '\n' << "\tRepoPac [PATH ...][OPTIONS]" << '\n';
	std::cout << "Description:" << '\n' << "\tRepoPac can help you package repository's content" << '\n';
	std::cout << "Options:" << '\n';
	std::cout << "\t-h, --help\tShow this help and exit" << '\n';
	std::cout << "\t-v, --version\tShow version and exit" << '\n';

	std::cout << "Arguments:" << '\n';
	std::cout << "\tOne directories/One or more files(default: .)" << '\n';
}

bool pexists(const fs::path& p) {
	std::error_code ec;
	return fs::exists(p, ec);
}

bool is_dir(const fs::path& p) {
	std::error_code ec;
	return fs::is_directory(p, ec);
}

bool is_file(const fs::path& p) {
	std::error_code ec;
	return fs::is_regular_file(p, ec);
}

bool is_git_repo(const fs::path& dir) {
	return fs::exists(dir / ".git") && fs::is_directory(dir / ".git");
}

void print_file(std::ostringstream& out, const fs::path& p) {
	out << "### File: " << p.string() << "\n";

	// using extention to print file type
	auto ext = p.extension().string();
	if (ext == ".json") out << "```json\n";
	else if (ext == ".js") out << "```javascript\n";
	else if (ext == ".cpp" || ext == ".hpp") out << "```cpp\n";
	else out << "```\n";

	// open file and put it into out
	std::error_code ec;
	const auto sz = fs::file_size(p, ec);
	std::ifstream ifs(p, std::ios::in);
	if (!ifs) {
		std::cerr << "(Could not open file)\n";
	}
	else if (sz <= kMaxBytes) {
		out << ifs.rdbuf() << '\n';
	}
	else {
		// read 16KB and turncat
		std::vector<char> buf(kMaxBytes);
		ifs.read(buf.data(), static_cast<std::streamsize>(buf.size()));
		out << std::string(buf.data(), static_cast<std::size_t>(ifs.gcount()));
		out << "\n... (truncated; original " << sz << " bytes, showing first "
			<< kMaxBytes << " bytes)\n";
	}

	out << "```\n\n";
}

void collect_files(const fs::path& root, std::vector<fs::path>& files) {
	std::error_code ec;
	if (!fs::exists(root, ec)) return;

	if (fs::is_directory(root, ec)) {
		std::vector<fs::directory_entry> entries(
			fs::directory_iterator(root, ec), {}
		);
		std::sort(entries.begin(), entries.end(),
			[](auto& a, auto& b) { return a.path().filename() < b.path().filename(); });
		for (auto& e : entries) {
			if (e.is_directory()) collect_files(e.path(), files);
			else if (e.is_regular_file()) files.push_back(e.path());
		}
	}
	else if (fs::is_regular_file(root, ec)) {
		files.push_back(root);
	}
}

void print_structure(std::ostringstream& out, const fs::path& root, int depth = 0) {
	std::error_code ec;
	if (!fs::exists(root, ec)) return;

	if (fs::is_directory(root, ec)) {
		std::vector<fs::directory_entry> entries(
			fs::directory_iterator(root, ec), {}
		);
		std::sort(entries.begin(), entries.end(),
			[](auto& a, auto& b) { return a.path().filename() < b.path().filename(); });

		for (auto& e : entries) {
			std::string indent(depth * 2, ' ');
			auto name = e.path().filename().string();
			if (e.is_directory()) {
				out << indent << name << "/\n";
				print_structure(out, e.path(), depth + 1);
			}
			else if (e.is_regular_file()) {
				out << indent << name << "\n";
			}
		}
	}
}

void print_dir(std::ostringstream& out, const fs::path& p) {
	out << "# Repository Context\n\n";

	out << "## File System Location\n\n";
	out << fs::absolute(p).generic_string() << "\n\n";

	if (is_git_repo(p)) {
		out << "## Git Info\n\n";
		// TODO: print git content
	}
	else {
		out << "Not a git repository\n\n";
	}

	out << "## Structure\n";
	out << "```\n";
	print_structure(out, p);
	out << "```\n\n";

	// collect and print files content
	std::vector<fs::path> files;
	collect_files(p, files);

	if (!files.empty()) {
		out << "## File Contents\n\n";
		for (auto& f : files) {
			print_file(out, f);
		}
	}
}

int main(int argc, char* argv[]) {
	std::vector<std::string> paths;
	std::ostringstream out;

	// parse very basic flags
	for (int i = 1; i < argc; ++i) {
		std::string arg = argv[i];
		if (arg == "-h" || arg == "--help") {
			print_help();
			return 0;
		}
		else if (arg == "-v" || arg == "--version") {
			std::cout << kToolName << " " << kVersion << "\n";
			return 0;
		}
		else if (!arg.empty() && arg[0] == '-') {
			std::cerr << "Unknown option: " << arg << "\n";
			std::cerr << "Use -h or --help for usage.\n";
			return 1;
		}
		else {
			paths.push_back(arg);
		}
	}

	if (paths.empty()) {
		paths.push_back("."); // default to current directory
	}

	for (const auto& path : paths) {
		fs::path p = path;
		if (!pexists(p)) {
			std::cerr << path << "is not a valid directory or file\n";
		}
		else if (is_dir(p)) {
			print_dir(out, p);
		}
		else if (is_file(p)) {
			out << "## File Contents\n\n";
			print_file(out, p);
		}
	}

	std::cout << out.str();

	return 0;
}