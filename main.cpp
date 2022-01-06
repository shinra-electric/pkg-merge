/**
 * Original Author: Tustin & 0x199
 * Original github: https://github.com/Tustin/pkg-merge
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

namespace fs = std::filesystem;

using std::string;
using std::map;
using std::vector;

struct Package {
    int					part;
    fs::path			file;
    vector<Package>		parts;
    bool operator < (const Package& rhs) const {
        return part < rhs.part;
    }
};

struct Files {
    int					part;
    string              title_id;
    fs::path			file;
};

struct cmpSort {
    bool operator()(int a, int b) const {
        return a < b;
    }
};

const char PKG_MAGIC[4] = { 0x7F, 0x43, 0x4E, 0x54 };

void merge(map<string, Package> packages) {
    for (auto & root : packages) {
        auto pkg = root.second;

        // Before we start, we need to sort the lists properly
        // std::sort(pkg.parts.begin(), pkg.parts.end());

        size_t pieces = pkg.parts.size();
        auto title_id = root.first.c_str();

        printf("[work] beginning to merge %d %s for package %s...\n", pieces, pieces == 1 ? "piece" : "pieces", title_id);

        string merged_file_name = root.first + "-merged.pkg";
        // string full_merged_file = pkg.file.parent_path().string() + "\\" + merged_file_name;

        // fs::path dir (pkg.file.parent_path().string());
        fs::path file (merged_file_name);

        fs::path _full_merged_file = pkg.file.parent_path() / file;
        string full_merged_file = _full_merged_file.string();

        if (fs::exists(full_merged_file)) {
            fs::remove(full_merged_file);
        }

        printf("\t[work] copying root package file to new file...");
        auto merged_file = fs::path(full_merged_file);

        // Deal with root file first
        fs::copy_file(pkg.file, merged_file, fs::copy_options::update_existing);
        printf("done\n");

        // Using C API from here on because it just works and is fast
        FILE *merged = fopen(full_merged_file.c_str(), "a+b");

        // Now all the pieces...
        for (auto & part : pkg.parts) {
            FILE *to_merge = fopen(part.file.string().c_str(), "r+b");

            auto total_size = fs::file_size(part.file);
            assert(total_size != 0);
            char buffer[1024 * 512];
            uintmax_t copied = 0;

            size_t read_data;
            while ((read_data = fread(buffer, sizeof(char), sizeof(buffer), to_merge)) > 0)
            {
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
#ifndef _DEBUG
    if (argc != 2) {
        std::cout << "No pkg directory supplied\nUsage: pkg-merge.exe <directory>" << std::endl;
        return 1;
    }
    string dir = argv[1];
#else
    string dir = "//doom/";
#endif // !_DEBUG

    if (!fs::is_directory(dir)) {
        printf("[error] argument '%s' is not a directory\n", dir.c_str());
        return 1;
    }

    map<string, Package> packages;
    map<int, Files, cmpSort> files;

    printf("Reading files...");

    for (auto & file : fs::directory_iterator(dir)) {
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
        char* ptr = NULL;
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
        _file.part = (int)pkg_piece;
        _file.file = file.path();
        _file.title_id = title_id;
        files.insert(std::pair<int, Files>(pkg_piece, _file));
    }

    // Add files to Package struct like before.
    for (auto & file : files) {
        if(file.first == 0) {
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

    merge(packages);
    printf("\n[success] completed\n");
    return 0;
}
