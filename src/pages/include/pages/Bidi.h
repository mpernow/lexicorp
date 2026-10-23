#pragma once

#include <string>

// `utf8` wrapped in Unicode isolate marks (FSI ... PDI), for a word or name
// inside an interface sentence. The isolate takes its direction from its own
// text and doesn't affect the text around it, so a Kurdish word in an English
// sentence doesn't drag the quotes and numbers after it to the wrong side.
// The marks are written as escapes: raw bidi controls are invisible in an
// editor, and GCC's -Wbidi-chars flags them ("Trojan Source").
inline std::string isolate(const std::string &utf8) {
  return "\u2068" + utf8 + "\u2069";
}
