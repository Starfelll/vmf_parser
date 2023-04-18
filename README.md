# VMF Parser (Valve Map Format) written in C
Simple utility for parsing and saving the VMF files. Useful for procedural map editting

An example code is presented in ```main.c```

## About
I've written that parser in July, 2022. The main idea was to automate the process of linking window brushes to window frames and bars,
to make them breakable. It takes a lot of time to do it manualy, thus I've decided to write a C program to do it for me. I'm not happy with
how it works, a regular parsing workflow is to lexe the character stream on demand of a tokenizer and tokenize it on demand of a parser, so
it goes like: ```parser<-tokenizer<-lexer<-character stream```, where as this parser lexes, tokenizes and emits a token. The parser changes
it's state according to the supplied token. For such simple format as VMF it is fine and works nicely, but I cannot imagine using such workflow
in case of an FGD files or basically - any other text based format. It's quite horrible...
