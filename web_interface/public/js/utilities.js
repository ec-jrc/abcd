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

function generate_slider (id, checked) {
    var slider = $("<label>");
    slider.addClass("custom-slider");
    if (checked) {
        slider.append($("<input>", {type: "checkbox", id: id, checked: "checked"}));
    } else {
        slider.append($("<input>", {type: "checkbox", id: id}));
    }
    slider.append($("<span>").addClass("cursor"));

    return slider;
}

create_and_download_file = function (contents, file_name, file_type) {
    const data = new Blob([contents], {type: 'text/' + file_type});

    var click_event = new MouseEvent("click", {"view": window,
                                               "bubbles": true,
                                               "cancelable": false});

    var a = document.createElement('a');
    a.href = window.URL.createObjectURL(data);
    a.download = file_name;
    a.textContent = 'Download file!';
    a.dispatchEvent(click_event);
}
