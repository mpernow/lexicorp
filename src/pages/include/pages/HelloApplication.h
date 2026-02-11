#include <Wt/WApplication.h>
#include <Wt/WText.h>
#include <Wt/WTextArea.h>

class HelloApplication : public Wt::WApplication {
public:
  HelloApplication(const Wt::WEnvironment &env);

private:
  Wt::WTextArea *textEdit_;
  Wt::WText *greeting_;
};
