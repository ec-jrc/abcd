// (C) Copyright 2016 Cristiano Lino Fontana
//
// This file is part of ABCD.
//
// ABCD is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// ABCD is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with ABCD.  If not, see <http://www.gnu.org/licenses/>.

'use strict';

function generate_update_events (module) {
    var previous_events = {};

    return function (events) {
        if (! _.isEqual(previous_events, events)) {
            previous_events = events;

            if (events.length > 0) {
                $("#errorlog_" + module + "_n").text(" (" + events.length + ")");
            } else {
                $("#errorlog_" + module + "_n").text('');
            }

            if (events.length > 0) {
                const message = events[0];

                const type = message["type"];
                const timestamp = moment(message["timestamp"]);
                const description = message[type];

                var counts = []
                counts[0] = {T: type, d: description, n: 1|0, t: timestamp};

                for (var i = 1; i < events.length; i++) {
                    const message = events[i];

                    const type = message["type"];
                    const timestamp = moment(message["timestamp"]);
                    const description = message[type];

                    const last_index = counts.length - 1;

                    if (counts[last_index].d === description)
                    {
                        counts[last_index].n += 1|0;
                        counts[last_index].t = timestamp;
                    }
                    else
                    {
                        counts.push({T: type, d: description, n: 1|0, t: timestamp});
                    }
                }

                var ol = $("<ol>");

                for (var i = 0; i < counts.length; i++) {
                    var li = $("<li>");

                    if (counts[i].n > 1) {
                        li.text("[" + counts[i].t.format('LLL') + "] " + counts[i].d + " (x " + counts[i].n + ")");
                    } else {
                        li.text("[" + counts[i].t.format('LLL') + "] " + counts[i].d);
                    }

                    if (counts[i].T === 'error') {
                        li.addClass("bad_status");
                    }

                    ol.append(li);
                }

                $("#errorlog_" + module).html(ol);
            }
        }
    }
}
