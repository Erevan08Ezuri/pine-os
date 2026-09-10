#pragma once

#include <cstddef>
#include <functional>
#include <string>
#include <string_view>

namespace Pine {
enum class InputType { Text, Multiline, Number, Search, Phone, Email, Url, Password };

class TextInputSession {
public:
  using Callback = std::function<void()>;
  TextInputSession(std::string& value, InputType type, Callback changed = {},
                   Callback submitted = {}, Callback blurred = {}, std::size_t maxLength = 65536);
  const std::string& value() const { return value_; }
  InputType type() const { return type_; }
  std::size_t cursor() const { return cursor_; }
  std::size_t selectionAnchor() const { return anchor_; }
  bool hasSelection() const { return cursor_ != anchor_; }
  void setCursor(std::size_t position);
  void setSelection(std::size_t anchor, std::size_t cursor);
  void selectAll();
  void insertText(std::string_view text);
  void replaceSelection(std::string_view text);
  void deleteBackward();
  void moveLeft(bool selecting = false);
  void moveRight(bool selecting = false);
  void submit();
  void notifyBlurred();
private:
  std::size_t previousCodepoint(std::size_t position) const;
  std::size_t nextCodepoint(std::size_t position) const;
  std::string filtered(std::string_view text) const;
  std::string& value_;
  InputType type_;
  Callback changed_, submitted_, blurred_;
  std::size_t maxLength_, cursor_{}, anchor_{};
};
}
