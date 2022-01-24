# File conversion

The [`ade2ascii.py`](./ade2ascii.py) python script and the [`ade2ascii`](./ade2ascii.c) C99 program convert the events files to an ASCII file with the format:

    #N      timestamp       qshort  qlong   channel
    0       3403941888      1532    1760    4
    1       3615693824      471     561     4
    2       4078839808      210     268     4
    3       4961184768      198     216     4
    4       6212482048      775     892     4
    ...     ...             ...     ...     ...

The [`ade2ascii.m`](./ade2ascii.m) script shows how to read the data files in Octave (and Matlab) and prints them in ASCII with the [`ade2ascii.py`](./ade2ascii.py) format.

The [`adw2ascii`](./ade2ascii.c) C99 program converts the waveforms files to an ASCII file in which the waveforms samples are written line by line.

The [`adr2adeadw.py`](./adr2adeadw.py) python script extracts from raw files the events and the waveforms files.

The [`adr2configs.py`](./adr2configs.py) python script extracts from raw files the configurations.
