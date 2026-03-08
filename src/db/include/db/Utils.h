#pragma once

#include <concepts>
#include <sstream>
#include <string>
#include <unicode/unistr.h>
#include <vector>

namespace db::Utils {

template <typename T>
concept IsDbModel = requires(T m) {
  {
    T::schema()
  } -> std::same_as<std::vector<std::pair<std::string, std::string>>>;
  { T::tableName } -> std::convertible_to<std::string_view>;
};

template <typename T>
  requires IsDbModel<T>
std::string createTableSql() {
  std::ostringstream sql;
  sql << "CREATE TABLE IF NOT EXISTS " << T::tableName << "(";

  auto fields = T::schema();
  for (size_t i = 0; i < fields.size(); ++i) {
    sql << fields[i].first << " " << fields[i].second;
    if (i + 1 < fields.size())
      sql << ", ";
  }

  sql << ");";
  return sql.str();
}

inline std::string toUtf8(const std::wstring &wstr) {
  if (wstr.empty())
    return {};

  icu::UnicodeString ustr = icu::UnicodeString::fromUTF32(
      reinterpret_cast<const UChar32 *>(wstr.c_str()),
      static_cast<int32_t>(wstr.length()));

  std::string result;
  ustr.toUTF8String(result);

  return result;
}

inline std::wstring fromUtf8(const std::string &utf8) {
  icu::UnicodeString ustr = icu::UnicodeString::fromUTF8(utf8);
  std::wstring ws;
  ws.reserve(ustr.countChar32());

  for (int32_t i = 0; i < ustr.length();) {
    ws.push_back(static_cast<wchar_t>(ustr.char32At(i)));
    i += U16_LENGTH(ustr.char32At(i));
  }

  return ws;
}

} // namespace db::Utils
