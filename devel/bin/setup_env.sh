#!/bin/sh

# Licensed to the Apache Software Foundation (ASF) under one or more
# contributor license agreements.  See the NOTICE file distributed with
# this work for additional information regarding copyright ownership.
# The ASF licenses this file to You under the Apache License, Version 2.0
# (the "License"); you may not use this file except in compliance with
# the License.  You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# Set up the following environment variables to build a Clownfish project with
# an uninstalled Clownfish source tree. Useful for development.
#
# - PATH
# - CPATH
# - LIBRARY_PATH
# - LD_LIBRARY_PATH
# - CLOWNFISH_INCLUDE
# - PERL5LIB
#
# Usage: source setup_env.sh [path to clownfish]

contains() {
    string="$1"
    substring="$2"
    test "${string#*$substring}" != "$string"
}

add_to_path() {
    path="$1"
    dir="$2"
    if [ -z "$path" ]; then
        echo "$dir"
    elif ! contains ":$path:" ":$dir:"; then
        echo "$dir:$path"
    else
        echo "$path"
    fi
}

if [ -n "$1" ]; then
    base_dir="$1"
elif [ -n "$BASH_SOURCE" ]; then
    # Only works with bash.
    script_dir=`dirname "$BASH_SOURCE"`
    base_dir=`cd "$script_dir/../.." && pwd`
else
    echo "Usage: source setup_env.sh path_to_clownfish_source"
    return 1 2>/dev/null || exit 1
fi

if [ ! -d "$base_dir/compiler" ] || [ ! -d "$base_dir/runtime" ]
then
    echo "Doesn't look like a Clownfish source directory: $base_dir"
    return 1 2>/dev/null || exit 1
fi

compiler_dir=$base_dir/compiler
runtime_dir=$base_dir/runtime

export PATH=`add_to_path "$PATH" "$compiler_dir/c"`
export CPATH=`add_to_path "$CPATH" "$runtime_dir/perl/xs"`
export LIBRARY_PATH=`add_to_path "$LIBRARY_PATH" "$runtime_dir/c"`
export CLOWNFISH_INCLUDE=`add_to_path "$CLOWNFISH_INCLUDE" "$runtime_dir/c/autogen/share/clownfish/include"`
export PERL5LIB=`add_to_path "$PERL5LIB" "$compiler_dir/perl/blib/arch"`
export PERL5LIB=`add_to_path "$PERL5LIB" "$compiler_dir/perl/blib/lib"`
export PERL5LIB=`add_to_path "$PERL5LIB" "$runtime_dir/perl/blib/arch"`
export PERL5LIB=`add_to_path "$PERL5LIB" "$runtime_dir/perl/blib/lib"`

case `uname` in
    MINGW*|CYGWIN*)
        export PATH=`add_to_path "$PATH" "$runtime_dir/c"`
	;;
    Darwin*)
        ;;
    *)
        export LD_LIBRARY_PATH=`add_to_path "$LD_LIBRARY_PATH" "$runtime_dir/c"`
esac

