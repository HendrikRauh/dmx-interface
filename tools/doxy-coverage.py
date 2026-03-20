#!/usr/bin/env python

# -*- mode: python; coding: utf-8 -*-

# Based on https://github.com/davatorium/doxy-coverage
# which was forked from https://github.com/alobbs/doxy-coverage
# and modified for use in this project.

# All files in doxy-coverage are Copyright 2014 Alvaro Lopez Ortega.
#
#   Authors:
#     * Alvaro Lopez Ortega <alvaro@gnu.org>
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
#     * Redistributions of source code must retain the above copyright
#       notice, this list of conditions and the following disclaimer.
#
#     * Redistributions in binary form must reproduce the above
#       copyright notice, this list of conditions and the following
#       disclaimer in the documentation and/or other materials provided
#       with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.


__author__ = "Alvaro Lopez Ortega"
__email__ = "alvaro@alobbs.com"
__copyright__ = "Copyright (C) 2014 Alvaro Lopez Ortega"

from filecmp import cmp
import os
import subprocess
import sys
import argparse
import xml.etree.ElementTree as ET

# Defaults
ACCEPTABLE_COVERAGE = 80

# Global
ns = None


def ERROR(*objs):
    print("ERROR: ", *objs, end="\n", file=sys.stderr)


def FATAL(*objs):
    ERROR(*objs)
    sys.exit(0 if ns.no_error else 1)


def generate_docs():
    print("Generating Doxygen documentation...")
    proc = subprocess.run(
        "doxygen Doxyfile",
        shell=True,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )
    if proc.returncode == 0:
        print("Documentation generated")
    else:
        FATAL("Failed to generate documentation. Exiting.")


def parse_file(fullpath):
    tree = ET.parse(fullpath)

    sourcefile = None
    definitions = {}

    for definition in tree.findall("./compounddef//memberdef"):
        # Should it be documented
        if definition.get("kind") == "function" and definition.get("static") == "yes":
            continue

        # Is the definition documented?
        documented = False
        for k in ("briefdescription", "detaileddescription", "inbodydescription"):
            if definition.findall(f"./{k}/"):
                documented = True
                break

        # Name
        d_def = definition.find("./definition")
        d_nam = definition.find("./name")

        if not sourcefile:
            l = definition.find("./location")
            if l is not None:
                sourcefile = l.get("file")

        if d_def is not None:
            name = d_def.text
        elif d_nam is not None:
            name = d_nam.text
        else:
            name = definition.get("id")

        # Aggregate
        definitions[name] = documented

    if not sourcefile:
        sourcefile = fullpath

    return (sourcefile, definitions)


def parse(path):
    index_fp = os.path.join(path, "index.xml")
    if not os.path.exists(index_fp):
        FATAL("Documentation not present. Exiting.", index_fp)

    tree = ET.parse(index_fp)

    files = {}
    for entry in tree.findall("compound"):
        if entry.get("kind") == "dir":
            continue

        file_fp = os.path.join(path, f"{entry.get('refid')}.xml")
        sourcefile, definitions = parse_file(file_fp)

        if definitions != {}:
            files[sourcefile] = definitions

    return files


def report(files, include_files, summary_only):
    files_sorted = sorted(files.keys())

    if len(include_files) != 0:
        files_sorted = list(filter(lambda f: f in include_files, files_sorted))

    if len(files_sorted) == 0:
        FATAL("No files to report on. Exiting.")

    files_sorted.reverse()

    total_yes = 0
    total_no = 0

    for f in files_sorted:
        defs = files[f]

        doc_yes = len([d for d in defs.values() if d])
        doc_no = len([d for d in defs.values() if not d])
        doc_per = doc_yes * 100.0 / (doc_yes + doc_no)

        total_yes += doc_yes
        total_no += doc_no

        if not summary_only:
            print(f"{int(doc_per):3d}% - {f} - ({doc_yes} of {doc_yes + doc_no})")

        if None in defs:
            del defs[None]
        if not summary_only:
            defs_sorted = sorted(defs.keys())
            for d in defs_sorted:
                if not defs[d]:
                    print("\t", d)

    total_all = total_yes + total_no
    total_percentage = total_yes * 100 / total_all
    print(
        f"{"" if summary_only else "\n"}{int(total_percentage)}% API documentation coverage"
    )
    return (ns.threshold - total_percentage, 0)[total_percentage > ns.threshold]


def main():
    # Arguments
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "dir", action="store", help="Path to Doxygen's XML doc directory"
    )
    parser.add_argument(
        "include_files",
        action="extend",
        nargs="*",
        help="List of files to check coverage for (Default: all files)",
        type=str,
        default=[],
    )
    parser.add_argument(
        "--no-error",
        action="store_true",
        help="Do not return error code after execution",
    )
    parser.add_argument(
        "--summary-only",
        action="store_true",
        help="Only print the summary of the coverage report, without listing the coverage of each file",
    )
    parser.add_argument(
        "--threshold",
        action="store",
        help=f"Min acceptable coverage percentage (Default: {ACCEPTABLE_COVERAGE})",
        default=ACCEPTABLE_COVERAGE,
        type=int,
    )
    parser.add_argument(
        "--generate-docs",
        action="store_true",
        help="Generate Doxygen documentation before checking coverage",
    )

    global ns
    ns = parser.parse_args()
    if not ns:
        FATAL("ERROR: Couldn't parse parameters")

    if ns.generate_docs:
        generate_docs()

    # Parse
    files = parse(ns.dir)

    # Print report
    err = report(files, ns.include_files, ns.summary_only)
    if ns.no_error:
        return

    sys.exit(round(err))


if __name__ == "__main__":
    main()
