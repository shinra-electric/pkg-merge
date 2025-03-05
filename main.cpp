/**
 * Original Author: Tustin & 0x199
 * Original github: https://github.com/Tustin/pkg-merge
 *
 * Libraries:
 *      - https://github.com/btzy/nativefiledialog-extended
 */

#include <cstdio>
#include <string>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <map>
#include <list>
#include <cassert>
#include <vector>
#include <cstring>
#include <nfd.h>

using namespace std;

namespace fs = std::filesystem;

using std::string;
using std::map;
using std::vector;

struct Package {
    int part;
    fs::path file;
    vector<Package> parts;

    bool operator<(const Package &rhs) const {
        return part < rhs.part;
    }
};

struct Files {
    int part;
    string title_id;
    fs::path file;
};

struct cmpSort {
    bool operator()(int a, int b) const {
        return a < b;
    }
};

const char PKG_MAGIC[4] = {0x7F, 0x43, 0x4E, 0x54};

char *get_argument_value(int argc, char *argv[], const char *arg_name) {
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], arg_name) == 0 && i + 1 < argc) {
            return argv[i + 1]; // Return next argument as value
        }
    }
    return NULL; // Return NULL if argument not found
}

void parse_arguments(
    int argc,
    char *argv[],
    std::string &input,
    std::string &output,
    const std::string &input_flag,
    const std::string &output_flag
) {
    input = "";
    output = "";

    int i = 1; // Start processing after program name

    while (i < argc) {
        if (argv[i] == input_flag && i + 1 < argc) {
            input = argv[i + 1];
            i += 2;
        } else if (argv[i] == output_flag && i + 1 < argc) {
            output = argv[i + 1];
            i += 2;
        } else if (input.empty()) {
            input = argv[i];
            i++;
        } else if (output.empty()) {
            output = argv[i];
            i++;
        } else {
            i++; // Ignore unexpected arguments
        }
    }

    // Default output to input if not set
    if (!input.empty() && output.empty()) {
        output = input;
    }
}

void merge(map<string, Package> packages, const string &out_dir) {
    for (auto &root: packages) {
        auto pkg = root.second;

        size_t pieces = pkg.parts.size();
        auto title_id = root.first.c_str();

        printf("[work] beginning to merge %d %s for package %s...\n", pieces, pieces == 1 ? "piece" : "pieces", title_id);

        string merged_file_name = root.first + "-merged.pkg";
        fs::path merged_file_path = fs::path(out_dir) / merged_file_name; // Use output directory

        if (fs::exists(merged_file_path)) {
            fs::remove(merged_file_path);
        }

        printf("\t[work] copying root package file to new file...");
        fs::copy_file(pkg.file, merged_file_path, fs::copy_options::update_existing);
        printf("done\n");

        FILE *merged = fopen(merged_file_path.string().c_str(), "a+b");

        // Merge parts into the output file
        for (auto &part: pkg.parts) {
            FILE *to_merge = fopen(part.file.string().c_str(), "r+b");
            auto total_size = fs::file_size(part.file);
            assert(total_size != 0);

            char buffer[1024 * 512];
            uintmax_t copied = 0;

            size_t read_data;
            while ((read_data = fread(buffer, sizeof(char), sizeof(buffer), to_merge)) > 0) {
                fwrite(buffer, sizeof(char), read_data, merged);
                copied += read_data * sizeof(char);
                auto percentage = ((double)copied / (double)total_size) * 100;
                printf("\r\t[work] merged %llu/%llu bytes (%.0lf%%) for part %d...", copied, total_size, percentage, part.part);
            }
            fclose(to_merge);

            printf("done\n");
        }
        fclose(merged);
    }
}

int main(int argc, char *argv[]) {
    //////////////////////////////////
    std::string dir, out_dir;

    // Dynamic flag names
    std::string input_flag = "-i";
    std::string output_flag = "-o";

    parse_arguments(argc, argv, dir, out_dir, input_flag, output_flag);

    if (dir.empty()) {
        // Open dialog
        NFD_Init();

        nfdnchar_t *outPath;
        nfdresult_t result = NFD_PickFolderN(&outPath, NULL);

        if (result == NFD_OKAY) {
            puts("Success!");
            // puts(outPath);

            // I don't know why we need to use wstring for Windows but not for MacOS. Not tested for Linux.
#ifdef _WIN32
            wstring ws(outPath);
            string _dir(ws.begin(), ws.end());
            dir = _dir;
#elif _WIN64
            wstring ws(outPath);
            string _dir(ws.begin(), ws.end());
            dir = _dir;
#elif __APPLE__ || __MACH__
            dir = outPath;
#elif __linux__ || __unix || __unix__
            dir = outPath;
#endif
            NFD_FreePathN(outPath);
        } else if (result == NFD_CANCEL) {
            puts("User pressed cancel.");
            return 1;
        } else {
            printf("Error: %s\n", NFD_GetError());
            return 1;
        }

        NFD_Quit();
    }

    if (out_dir.empty()) {
        out_dir = dir;
    }

    if (!fs::is_directory(dir)) {
        printf("[error] argument '%s' is not a directory\n", dir.c_str());
        return 1;
    }

    map<string, Package> packages;
    map<int, Files, cmpSort> files;

    printf("Reading files...\n");

    for (auto &file: fs::directory_iterator(dir)) {
        string file_name = file.path().filename().string();

        if (file.path().extension() != ".pkg") {
            printf("[warn] '%s' is not a PKG file. skipping...\n", file_name.c_str());
            continue;
        }

        if (file_name.find("-merged") != string::npos) continue;

        size_t found_part_begin = file_name.find_last_of("_") + 1;
        size_t found_part_end = file_name.find_first_of(".");
        string part = file_name.substr(found_part_begin, found_part_end - found_part_begin);
        string title_id = file_name.substr(0, found_part_begin - 1);
        char *ptr = NULL;
        auto pkg_piece = strtol(part.c_str(), &ptr, 10);
        if (ptr == NULL) {
            printf("[warn] '%s' is not a valid piece (fails integer conversion). skipping...\n", part.c_str());
            continue;
        }

        /**
         * TODO: This throws compile error on Windows. Not sure why.
         * Check if it's root PKG
         */
        //        if(pkg_piece == 0){
        //            std::ifstream ifs(file, std::ios::binary);
        //            char magic[4];
        //            ifs.read(magic, sizeof(magic));
        //            ifs.close();
        //
        //            if (memcmp(magic, PKG_MAGIC, sizeof(PKG_MAGIC)) != 0) {
        //                printf("[warn] assumed root PKG file '%s' doesn't match PKG magic (is %s, wants %s). skipping...\n", file_name.c_str(), magic, PKG_MAGIC);
        //                continue;
        //            }
        //        }

        auto _file = Files();
        _file.part = (int) pkg_piece;
        _file.file = file.path();
        _file.title_id = title_id;
        files.insert(std::pair<int, Files>(pkg_piece, _file));
    }

    // Add files to Package struct like before.
    for (auto &file: files) {
        if (file.first == 0) {
            auto package = Package();
            package.part = 0;
            // package.file = fs::path(path + file.second.file.relative_path().string());
            package.file = (fs::path(dir) / file.second.file.filename()).string();
            packages.insert(std::pair<string, Package>(file.second.title_id, package));

            continue;
        }

        auto it = packages.find(file.second.title_id);

        auto pkg = &it->second;
        auto piece = Package();
        // piece.file = fs::path(path + file.second.file.relative_path().string());
        piece.file = (fs::path(dir) / file.second.file.filename()).string();
        piece.part = file.first;
        pkg->parts.push_back(piece);
    }

    merge(packages, out_dir);
    printf("\n[success] completed\n");
    return 0;
}
