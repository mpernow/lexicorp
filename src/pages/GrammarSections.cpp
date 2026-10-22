#include "pages/GrammarSections.h"

#include <Wt/WDialog.h>
#include <Wt/WPushButton.h>
#include <Wt/WTemplate.h>
#include <Wt/WText.h>

#include <cctype>
#include <memory>
#include <regex>
#include <string_view>
#include <vector>

namespace {
std::string escape(std::string_view text) {
  std::string escaped;
  for (char c : text) {
    switch (c) {
    case '&':
      escaped += "&amp;";
      break;
    case '<':
      escaped += "&lt;";
      break;
    case '>':
      escaped += "&gt;";
      break;
    case '"':
      escaped += "&quot;";
      break;
    default:
      escaped += c;
    }
  }
  return escaped;
}

std::string trim(std::string_view text) {
  auto first = text.find_first_not_of(" \t\r");
  if (first == std::string_view::npos)
    return "";
  auto last = text.find_last_not_of(" \t\r");
  return std::string{text.substr(first, last + 1 - first)};
}

// Escaped, with **bold**, *italic* and `code`.
std::string inlineXhtml(std::string_view text) {
  static const std::regex code{"`([^`]+)`"};
  static const std::regex bold{R"(\*\*([^*]+)\*\*)"};
  static const std::regex italic{R"(\*([^*\s][^*]*)\*)"};
  auto html = escape(text);
  html = std::regex_replace(html, code, "<code>$1</code>");
  html = std::regex_replace(html, bold, "<strong>$1</strong>");
  return std::regex_replace(html, italic, "<em>$1</em>");
}

std::vector<std::string> tableCells(std::string_view line) {
  auto row = trim(line);
  if (row.starts_with('|'))
    row.erase(0, 1);
  if (row.ends_with('|'))
    row.pop_back();
  std::vector<std::string> cells;
  std::size_t start = 0;
  while (true) {
    auto end = row.find('|', start);
    cells.push_back(trim(std::string_view{row}.substr(start, end - start)));
    if (end == std::string::npos)
      return cells;
    start = end + 1;
  }
}

bool isTableSeparator(std::string_view line) {
  static const std::regex separator{R"(\s*\|?(\s*:?-+:?\s*\|)*\s*:?-+:?\s*\|?\s*)"};
  return std::regex_match(line.begin(), line.end(), separator);
}

class MarkdownRenderer {
public:
  std::string render(const std::string &markdown) {
    std::vector<std::string> lines;
    std::size_t start = 0;
    while (start <= markdown.size()) {
      auto end = markdown.find('\n', start);
      if (end == std::string::npos)
        end = markdown.size();
      lines.push_back(markdown.substr(start, end - start));
      start = end + 1;
    }

    static const std::regex heading{R"(\s*#{1,6}\s+(.*))"};
    static const std::regex listItem{R"(( *)([-*+]|\d+[.)])\s+(.*))"};
    for (std::size_t i = 0; i < lines.size(); ++i) {
      const auto &line = lines[i];
      std::smatch match;
      if (trim(line).empty()) {
        closeBlocks();
      } else if (std::regex_match(line, match, heading)) {
        closeBlocks();
        mHtml += "<h6>" + inlineXhtml(trim(match.str(1))) + "</h6>";
      } else if (trim(line).starts_with('|') && i + 1 < lines.size() &&
                 isTableSeparator(lines[i + 1])) {
        closeBlocks();
        mHtml += "<table class=\"table table-sm table-bordered\"><thead><tr>";
        for (const auto &cell : tableCells(line))
          mHtml += "<th>" + inlineXhtml(cell) + "</th>";
        mHtml += "</tr></thead><tbody>";
        for (i += 2; i < lines.size() && trim(lines[i]).starts_with('|'); ++i) {
          mHtml += "<tr>";
          for (const auto &cell : tableCells(lines[i]))
            mHtml += "<td dir=\"auto\">" + inlineXhtml(cell) + "</td>";
          mHtml += "</tr>";
        }
        --i;
        mHtml += "</tbody></table>";
      } else if (std::regex_match(line, match, listItem)) {
        closeParagraph();
        bool numbered = std::isdigit(static_cast<unsigned char>(*match[2].first));
        addListItem(match.length(1), numbered ? "ol" : "ul",
                    inlineXhtml(match.str(3)));
      } else if (!mLists.empty()) {
        // A continuation of the list item above.
        closeParagraph();
        mHtml += " " + inlineXhtml(trim(line));
      } else {
        mHtml += mInParagraph ? " " : "<p>";
        mInParagraph = true;
        mHtml += inlineXhtml(trim(line));
      }
    }
    closeBlocks();
    return mHtml;
  }

private:
  struct List {
    std::size_t indent;
    std::string tag;
  };

  void addListItem(std::size_t indent, const std::string &tag,
                   const std::string &html) {
    while (!mLists.empty() && mLists.back().indent > indent)
      closeList();
    if (!mLists.empty() && mLists.back().indent == indent &&
        mLists.back().tag != tag)
      closeList();
    if (!mLists.empty() && mLists.back().indent == indent) {
      mHtml += "</li><li>";
    } else {
      // Nested in the open item, if any.
      mHtml += "<" + tag + "><li>";
      mLists.push_back({indent, tag});
    }
    mHtml += html;
  }

  void closeList() {
    mHtml += "</li></" + mLists.back().tag + ">";
    mLists.pop_back();
  }

  void closeParagraph() {
    if (mInParagraph)
      mHtml += "</p>";
    mInParagraph = false;
  }

  void closeBlocks() {
    closeParagraph();
    while (!mLists.empty())
      closeList();
  }

  std::string mHtml;
  bool mInParagraph = false;
  std::vector<List> mLists;
};

// `xhtml` as a WTemplate's text, each section id of `grammar` in it (other
// than `self`) replaced by a variable ${ref0}, ${ref1}, … The ids are added
// to `refs` in that order.
std::string withReferences(const std::string &xhtml,
                           const claude::GrammarReference &grammar,
                           const std::string &self,
                           std::vector<std::string> &refs) {
  auto isIdChar = [](char c) {
    return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '.' ||
           c == '-';
  };
  std::string text;
  std::size_t i = 0;
  while (i < xhtml.size()) {
    if (!isIdChar(xhtml[i])) {
      // WTemplate reads ${…} as a variable; $$ is a literal $.
      text += xhtml[i] == '$' ? "$$" : std::string(1, xhtml[i]);
      ++i;
      continue;
    }
    auto end = i;
    while (end < xhtml.size() && isIdChar(xhtml[end]))
      ++end;
    // A full stop after an id ends the sentence.
    auto idEnd = end;
    while (idEnd > i && xhtml[idEnd - 1] == '.')
      --idEnd;
    auto token = xhtml.substr(i, idEnd - i);
    if (token != self && grammar.section(token)) {
      text += "${ref" + std::to_string(refs.size()) + "}";
      refs.push_back(token);
      text += xhtml.substr(idEnd, end - idEnd);
    } else {
      text += xhtml.substr(i, end - i);
    }
    i = end;
  }
  return text;
}

std::string titleOf(const claude::GrammarSection &section) {
  return section.title.empty() ? section.id : section.title;
}

// The sections shown in one dialog, so Back can return to the previous one.
struct SectionDialog {
  Wt::WDialog *dialog;
  const claude::GrammarReference *grammar;
  std::vector<std::string> history;
  Wt::WPushButton *back;
};

void showIn(const std::shared_ptr<SectionDialog> &state) {
  const auto &id = state->history.back();
  const auto *section = state->grammar->section(id);
  auto *dialog = state->dialog;
  dialog->setWindowTitle(Wt::WString::fromUTF8(section ? titleOf(*section) : id));
  state->back->setHidden(state->history.size() < 2);

  auto *contents = dialog->contents();
  contents->clear();
  auto idLine = contents->addNew<Wt::WText>(Wt::WString::fromUTF8(id),
                                            Wt::TextFormat::Plain);
  idLine->setInline(false);
  idLine->setStyleClass("small text-body-secondary mb-2");
  if (!section)
    return;

  std::vector<std::string> refs;
  auto body = contents->addNew<Wt::WTemplate>();
  body->setStyleClass("grammar-section");
  body->setTemplateText(
      Wt::WString::fromUTF8(withReferences(markdownToXhtml(section->text),
                                           *state->grammar, id, refs)),
      Wt::TextFormat::XHTML);
  std::weak_ptr<SectionDialog> weak = state;
  for (std::size_t i = 0; i < refs.size(); ++i) {
    const auto *target = state->grammar->section(refs[i]);
    auto link = std::make_unique<Wt::WPushButton>(Wt::WString::fromUTF8(refs[i]));
    link->setStyleClass("btn-link p-0 align-baseline grammar-link");
    link->setToolTip(Wt::WString::fromUTF8(titleOf(*target)));
    link->clicked().connect([weak, ref = refs[i]] {
      if (auto state = weak.lock()) {
        state->history.push_back(ref);
        showIn(state);
      }
    });
    body->bindWidget("ref" + std::to_string(i), std::move(link));
  }
}
} // namespace

std::string markdownToXhtml(const std::string &markdown) {
  return MarkdownRenderer{}.render(markdown);
}

void showGrammarSection(Wt::WContainerWidget *owner,
                        const claude::GrammarReference &grammar,
                        const std::string &id) {
  auto dialog = owner->addChild(std::make_unique<Wt::WDialog>());
  dialog->addStyleClass("grammar-dialog");
  dialog->contents()->addStyleClass("grammar-dialog-body");
  dialog->setClosable(true);
  dialog->rejectWhenEscapePressed();

  auto back = dialog->footer()->addNew<Wt::WPushButton>("Back");
  back->setStyleClass("btn-secondary");
  auto close = dialog->footer()->addNew<Wt::WPushButton>("Close");
  close->setStyleClass("btn-primary");
  close->clicked().connect(dialog, &Wt::WDialog::accept);

  auto state = std::make_shared<SectionDialog>(
      SectionDialog{dialog, &grammar, {id}, back});
  back->clicked().connect([state] {
    if (state->history.size() > 1) {
      state->history.pop_back();
      showIn(state);
    }
  });
  // `state` lives as long as the dialog's connections.
  dialog->finished().connect([owner, dialog, state](Wt::DialogCode) {
    // Destroyed at the end of this handler, not during it.
    auto removed = owner->removeChild(dialog);
  });
  showIn(state);
  dialog->show();
}
