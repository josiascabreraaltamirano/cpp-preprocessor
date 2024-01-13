#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

using namespace std;
using filesystem::path;

path operator""_p(const char* data, std::size_t sz) {
    return path(data, data + sz);
}

// напишите эту функцию
bool PreprocessInner(const path& file_path, istream& source, ostream& destine,
					 						const vector<path>& include_directories) {
	static regex include_version1{R"/(\s*#\s*include\s*"([^"]*)"\s*)/"};
	static regex include_version2{R"/(\s*#\s*include\s*<([^>]*)>\s*)/"};

	string line;
	int n_line = 0;

	while (getline(source, line)) {
		++n_line;
		smatch m1, m2;
		if (!regex_match(line, m1, include_version1) && !regex_match(line, m2, include_version2)) {
			destine << line << '\n';
		} else {
			ifstream input;
			path include_file_name;
			path include_file_possible_path;

			if (!m1.empty()) {
				include_file_name = string(m1[1]);
				include_file_possible_path = file_path.parent_path() / include_file_name;
				input.open(include_file_possible_path);
			} else {
				include_file_name = string(m2[1]);
			}

			for (const auto& dir : include_directories) {
				if (input.is_open() && input) {
					break;
				}
				include_file_possible_path = dir / include_file_name;
				input.open(include_file_possible_path);
			}

			if (!input) {
				cout << "unknown include file "s << include_file_name.string() << 
				" at file "s << file_path.string() << " at line "s << n_line << '\n';
				return false;
			}

			if (!PreprocessInner(include_file_possible_path, input, destine, include_directories)) {
				return false;
			}
		}
	}

	return true;

}

bool Preprocess(const path& in_file, const path& out_file, const vector<path>& include_directories) {
	ifstream fin(in_file);
	if (!fin) {
		cerr << in_file << endl;
		return false;
	}

	ofstream fout(out_file);
	if (!fout) {
		cerr << out_file << endl;
		return false;
	}
    
    return PreprocessInner(in_file, fin, fout, include_directories);
}

string GetFileContents(string file) {
    ifstream stream(file);

    // конструируем string по двум итераторам
    return {(istreambuf_iterator<char>(stream)), istreambuf_iterator<char>()};
}

void Test() {
    error_code err;
    filesystem::remove_all("sources"_p, err);
    filesystem::create_directories("sources"_p / "include2"_p / "lib"_p, err);
    filesystem::create_directories("sources"_p / "include1"_p, err);
    filesystem::create_directories("sources"_p / "dir1"_p / "subdir"_p, err);

    {
        ofstream file("sources/a.cpp");
        file << "// this comment before include\n"
                "#include \"dir1/b.h\"\n"
                "// text between b.h and c.h\n"
                "#include \"dir1/d.h\"\n"
                "\n"
                "int SayHello() {\n"
                "    cout << \"hello, world!\" << endl;\n"
                "#   include<dummy.txt>\n"
                "}\n"s;
    }
    {
        ofstream file("sources/dir1/b.h");
        file << "// text from b.h before include\n"
                "#include \"subdir/c.h\"\n"
                "// text from b.h after include"s;
    }
    {
        ofstream file("sources/dir1/subdir/c.h");
        file << "// text from c.h before include\n"
                "#include <std1.h>\n"
                "// text from c.h after include\n"s;
    }
    {
        ofstream file("sources/dir1/d.h");
        file << "// text from d.h before include\n"
                "#include \"lib/std2.h\"\n"
                "// text from d.h after include\n"s;
    }
    {
        ofstream file("sources/include1/std1.h");
        file << "// std1\n"s;
    }
    {
        ofstream file("sources/include2/lib/std2.h");
        file << "// std2\n"s;
    }

    assert((!Preprocess("sources"_p / "a.cpp"_p, "sources"_p / "a.in"_p,
                                  {"sources"_p / "include1"_p,"sources"_p / "include2"_p})));

    ostringstream test_out;
    test_out << "// this comment before include\n"
                "// text from b.h before include\n"
                "// text from c.h before include\n"
                "// std1\n"
                "// text from c.h after include\n"
                "// text from b.h after include\n"
                "// text between b.h and c.h\n"
                "// text from d.h before include\n"
                "// std2\n"
                "// text from d.h after include\n"
                "\n"
                "int SayHello() {\n"
                "    cout << \"hello, world!\" << endl;\n"s;

    assert(GetFileContents("sources/a.in"s) == test_out.str());
}

int main() {
    Test();
}
