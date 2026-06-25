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
            // CC combine-mode (Gneiss-only; other plugins never send these). Appended so the
            // enum stays additive — the shared mapper/serializer is untouched.
            mtag_set_cc_absolute,
            mtag_set_cc_modulate,
        msg_tags_end,
        num_msg_tags = msg_tags_end - msg_tags_begin
    };
    
}
