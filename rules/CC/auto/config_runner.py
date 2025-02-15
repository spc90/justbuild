#!/usr/bin/python3

# Copyright 2022 Huawei Cloud Computing Technology Co., Ltd.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

import json
import re
from sys import argv
from re import sub, match

input_file = argv[1]
param_file = argv[2]
magic_string = argv[3]
at_only = argv[4] == "true"


with open(param_file) as f:
    param = json.loads(f.read())

# In many cases, CMake simply defines some variables (without any associated
# value). We handle this situation by assigning to the boolean True the empty
# string. Note that no False value should be found, because the right way to set
# a variable to False in the TARGETS file is to *do not mention* that variable
# at all.
for k, v in param.items():
    if isinstance(v, bool):
        param[k] = ""

with open(input_file) as i:
    with open("out", "w") as o:
        for line in i.readlines():
            if x := re.search(
                r"#(.*)(" + magic_string + r" )([ \t]*)([a-zA-Z0-9_]+)", line
            ):
                # get the VAR
                key = x.groups()[-1]
                if key in param:
                    line = sub(
                        f"{x.group()[1:]}",
                        f"{x.groups()[0]}define {x.groups()[2]}{key} {param[key]}",
                        line,
                    )
                else:
                    line = f"/* #undef {x.groups()[-1]} */\n" 
            if x := re.search(
                r"#(.*)(" + magic_string + "01 )([ \t]*)([a-zA-Z0-9_]+)", line
            ):
                # get the VAR
                key = x.groups()[-1]
                line = sub(
                    f"{x.group()[1:]}",
                    f"{x.groups()[0]}define {x.groups()[2]}{key} "
                    + str(1 if key in param else 0),
                    line,
                )
            if match("#[ \t]*define", line):
                if x := re.search(r"@([a-zA-Z0-9-_]+)@", line):
                    key = x.groups()[0]
                    line = sub(x.group(), param.get(key, ""), line)
                if not at_only:
                    if x := re.search(r"\${([a-zA-Z0-9-_]+)}", line):
                        key = x.groups()[0]
                        line = sub(r"\${" + key + r"}", param.get(key, ""), line)

            print(line, end="", file=o)
