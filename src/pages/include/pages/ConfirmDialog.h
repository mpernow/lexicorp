#pragma once

#include <Wt/WMessageBox.h>
#include <Wt/WPushButton.h>
#include <functional>
#include <memory>
#include <string>

// Asks before a destructive action: a modal box with Cancel and a red
// `confirmLabel` button, which runs `onConfirm`. The box is owned by `owner`
// and removed once either button is clicked.
inline void confirmDestructive(Wt::WObject *owner, const std::string &title,
                               const std::string &message,
                               const std::string &confirmLabel,
                               std::function<void()> onConfirm) {
  auto box = owner->addChild(std::make_unique<Wt::WMessageBox>(
      Wt::WString::fromUTF8(title), Wt::WString::fromUTF8(message),
      Wt::Icon::Warning, Wt::StandardButton::None));

  box->addButton("Cancel", Wt::StandardButton::Cancel)
      ->setStyleClass("btn-secondary");
  auto confirmButton = box->addButton(Wt::WString::fromUTF8(confirmLabel),
                                      Wt::StandardButton::Yes);
  confirmButton->setStyleClass("btn-danger busy-button");
  // Disabled in the browser at once (see "Slow actions" in CLAUDE.md); the
  // box is removed afterwards, so it never needs re-enabling.
  confirmButton->clicked().connect(confirmButton, &Wt::WWidget::disable);
  // No default button: Wt's theme adds btn-primary to it, which clashes
  // with its own style (grey text on blue until hovered).
  box->setEscapeButton(Wt::StandardButton::Cancel);

  box->buttonClicked().connect(
      [owner, box, onConfirm = std::move(onConfirm)](Wt::StandardButton b) {
        // Destroyed at the end of this handler, not during it: this lambda
        // and onConfirm belong to the box.
        auto removed = owner->removeChild(box);
        if (b == Wt::StandardButton::Yes)
          onConfirm();
      });
  box->show();
}
