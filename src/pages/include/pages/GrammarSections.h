#pragma once

#include "claude/GrammarReference.h"

#include <Wt/WContainerWidget.h>

#include <string>

// The Markdown of a grammar section as XHTML: headings, paragraphs, nested
// bullet and numbered lists, tables, **bold**, *italic* and `code`. Only
// what the reference uses; everything else is escaped text.
std::string markdownToXhtml(const std::string &markdown);

// Opens a dialog, owned by `owner`, with section `id` of `grammar` (which
// must outlive it). The section ids its text mentions ("see clitics.sets")
// open those sections in the same dialog, with a Back button.
void showGrammarSection(Wt::WContainerWidget *owner,
                        const claude::GrammarReference &grammar,
                        const std::string &id);
