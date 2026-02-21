#pragma once

#include <concepts>
#include <sstream>
#include <string>
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

} // namespace db::Utils
