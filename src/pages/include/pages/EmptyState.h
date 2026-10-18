#pragma once

#include <Wt/WAnchor.h>
#include <Wt/WContainerWidget.h>
#include <Wt/WLink.h>
#include <Wt/WText.h>
#include <memory>
#include <string>

// A centred "nothing here yet" message, optionally with a call-to-action link
// to the Insert Text page.
inline std::unique_ptr<Wt::WContainerWidget>
makeEmptyState(const std::string &message, bool withInsertLink) {
  auto box = std::make_unique<Wt::WContainerWidget>();
  box->setStyleClass("empty-state text-center text-body-secondary py-5");

  auto text = box->addNew<Wt::WText>(Wt::WString::fromUTF8(message),
                                     Wt::TextFormat::Plain);
  text->setInline(false);
  text->addStyleClass("mb-3");

  if (withInsertLink) {
    auto link = box->addNew<Wt::WAnchor>(
        Wt::WLink(Wt::LinkType::InternalPath, "/insert"), "Insert a text");
    link->setStyleClass("btn btn-primary");
  }
  return box;
}
