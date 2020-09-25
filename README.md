# cppillr
*by David Capello*

*cppillr* ([cpluspiller](https://translate.google.com/#view=home&op=translate&sl=en&tl=en&text=cpluspiller))
is an experimental tool to analyze C++ code.

> This software is in beta stage, it's useless in its current state,
> so please don't expect stability or usefulness. Use it at your own
> risk.

## Usage

    cppiller [command] [-options...] [files...]

Commands:

* `cppiller run file.cpp`: Tries to compile and run the given `file.cpp` file (and its dependencies)
* `cppiller docs`: Creates a markdown file with the documentation of the given files (Doxygen-like?)

Global Options:

* `-filelist file.txt`: The given file.txt must contain a list of files to be readed. It's like passing through the command line all the paths inside the given file.txt.
* `-showtokens`: For debugging purposes: It shows the tokens of all input files.
* `-showincludes`: For debugging purposes: It shows the #include files of all the input files.
* `-counttokens`: Prints a counter of the read number of tokens.
* `-countlines`: Prints a counter of the number of lines with tokens (non-blank lines).
* `-keywordstats`: Prints a counter for each kind of token used in the input files.
