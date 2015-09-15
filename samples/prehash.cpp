//
//  prehash.cpp
//  
//
//  Created by Carl-Henrik Skårstedt on 9/14/15.
//
//
//  Parse a file for instances of strings to hash (prefixed by
//  PHASH).

#ifdef _WIN32
#define _CRT_SECURE_NO_WARNINGS
#endif

#define STRUSE_IMPLEMENTATION
#include "struse.h"

#define PHASH(str, hash) (hash)

#define PHASH_SANDWICH PHASH("Sandwich");
#define PHASH_SALAD PHASH("Salad");

// maximum length (keyword must be same line)
#define PHASH_MAX_LENGTH 1024

// maximum additional margin
#define PHASH_MAX_MARGIN 128*1024

#define PHASH_EXTEND_CHARACTERS 12

bool prehash(const char *file) {
    if (FILE *f = fopen(file, "rb")) {
        fseek(f, 0, SEEK_END);
        size_t size = ftell(f);
        fseek(f, 0, SEEK_SET);
        size_t buffer_size = size + PHASH_MAX_MARGIN;
        if (void *buffer = malloc(buffer_size)) {
            fread(buffer, size, 1, f);
            fclose(f);
            
            // create an overlay string to start scanning
            strovl overlay((char*)buffer, (strl_t)buffer_size, (strl_t)size);
            
            // calculate hash of entire file
            unsigned int original_hash = overlay.fnv1a();
            
            // find all instances of PHASH token
            // extra quotes inserted to allow running tool over this file
            strref pattern("P""HASH(*{ \t}\"*@\"*{!\n\r/})");
            strref match;
            strown<PHASH_MAX_LENGTH> replace;
            while ((match = overlay.wildcard_after(pattern, match))) {
                // get the keyword to hash
                strref keyword = match.between('"', '"');
                
                // generate a replacement to the original string
                replace.sprintf("PHASH(\"" STRREF_FMT "\", 0x%08x)", STRREF_ARG(keyword), keyword.fnv1a());
                
                // exchange the match with the evaluated string
                overlay.exchange(match, replace.get_strref());
                
                // if about to run out of space, early out.
                if (overlay.len()>(overlay.cap()-PHASH_EXTEND_CHARACTERS))
                    break;
            }
            
            // check if there was a change to the data
            if (size != overlay.len() || original_hash != overlay.fnv1a()) {
                // write back if possible
                if ((f = fopen(file, "wb"))) {
                    fwrite(overlay.get(), overlay.len(), 1, f);
                    fclose(f);
                } else
                    return false;
            }
            return true;
        } else
            fclose(f);
    }
    return false;
}

int main(int argc, char **argv) {
    const char *file = "samples\\prehash.cpp";
    if (argc>1)
        file = argv[1];

    if (!prehash(file)) {
        printf("Failed to prehash \"%s\"\n", file);
        return 1;
    }
    return 0;
}
