#pragma once

namespace hvoya::midi_cc {

    enum MessageTags {
        msg_tags_begin = 7777,
            mtag_listen_to_pid = msg_tags_begin,
            mtag_learn_cc,
            mtag_clear_cc,
            mtag_invert_range,
            mtag_set_min,
            mtag_set_max,
        msg_tags_end,
        num_msg_tags = msg_tags_end - msg_tags_begin
    };
    
}
