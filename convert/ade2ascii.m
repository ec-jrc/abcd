#! /usr/bin/env octave

if numel(argv()) == 0
    printf("Usage: %s <file_name>\n", program_invocation_name());
    printf("\n");
    printf("Read and print an ABCD events file converting it to ASCII\n");
else
    file_name = argv(){1};
    
    file_ID = fopen(file_name, "r");
    
    raw_data = fread(file_ID, Inf, "uint64", 0, "ieee-le");
    raw_words = raw_data(2:2:end, 1);
    
    N = numel(raw_data) / 2;
    
    timestamps = raw_data(1:2:end, 1);
    qshorts = arrayfun(@(x) bitand(65535, bitshift(x, 0)), raw_words);
    qlongs = arrayfun(@(x) bitand(65535, bitshift(x, -16)), raw_words);
    channels = arrayfun(@(x) bitand(255, bitshift(x, -48)), raw_words);
    
    printf("#N\ttimestamp\tqshort\tqlong\tchannel\n")
    
    for i = 1:N
        printf("%d\t%d\t%d\t%d\t%d\n", i, timestamps(i), qshorts(i), qlongs(i), channels(i));
    endfor
endif
