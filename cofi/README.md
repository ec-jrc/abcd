# cofi - COincidence FIlter

`cofi` filters the DPP events allowing the passage of only the data in
coincidence with a reference channel in a specified time-window.

## Algorithm
The `cofi` module works on data packets generating groups of events in
coincidence with the reference channels. The applied algorithm is:

1. *Sort* all the events in a packet according to the timestamps.
2. *Select* the events generated by *reference channels*.
3. *Search backward* in the events array for events that did not originate from
   a reference channel. If the timestamps are outside the left time window stop
   the search. Store the selected events in the output buffer.
4. *Search forward* for events that did not originate from a reference channel.
   If the timestamps are outside the right time window, stop the search. Store
   the selected events in the output buffer.

## Coincidence windows
ASCII art diagram of the coincidence windows.
```

                 Events from           Event from a
                 other channels        reference channel
                 in the left           |
                 coincidence           |
                 window                |
                 |                     |
                 +----------+-----+    |
                 |          |     |    |
                 V          V     V    V

     3           2          2     1    0         1   1      3    2
---------------------------------------|----------------------------> timestamps
             |                         t0 <- Time zero of the     |
             |                         |     coincidence window   |
             |-------------------------|--------------------------|
             | Left coincidence window   Right coincidence window |
             |                                                    |
             |                                                    |
            -t_l <- Left edge                       Right edge -> t_r
                    of the window                of the window
```

## Output
The data format is the same as the rest of ABCD, but the events are grouped in
coincidence groups. The *first event* in a group is always an event from a
reference channel. The first event contains in the `group_counter` entry the
number of events that are part of this group. The following `group_counter`
events should be treated as part of the coincidence group.

## Program options
The program expect a list of the reference channels in the command line:
```
sh$ ./cofi [options] <reference_channels>
```
The optional arguments are:

- `-h`: Display the inline help that also shows the default values
- `-v`: Set verbose execution
- `-V`: Set verbose execution with more output
- `-A <address>`: Input socket address
- `-D <address>`: Output socket address
- `-T <period>`: Set base period in milliseconds
- `-w <coincidence_window>`: Use a symmetric coincidence window. Both the left
  as well as the right coincidence windows will have a *width* of
  `coincidence_window` nanoseconds.
- `-l <left_coincidence_window>`: Left coincidence window width in nanoseconds.
  This is the width of the left coincidence window (**i.e.** in general it is a
  positive number).
- `-r <right_coincidence_window>`: Right coincidence window width in nanoseconds.
  This is the width of the right coincidence window.
- `-L <left_coincidence_edge>`: Left edge of the left coincidence window in
  nanoseconds. This is the time value of the left edge of the left coincidence
  window (**i.e.** in general it is a negative value equal to
  `-1 * left_coincidence_window`).
- `-m <multiplicity>`: Event minimum multiplicity, excluding the reference
  channel. The minimum number of events in a coincidence groups that enable the
  forwarding of this group.
- `-n <ns_per_sample>`: Conversion factor of nanoseconds per timestamp sample.
- `-k`: Enable keep reference event, even if there are no coincidences.
  The reference events will be always forwarded but not the other events.