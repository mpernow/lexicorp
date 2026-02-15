#ifndef UTILS_H
#define UTILS_H

#include <sstream>
#include <string>

namespace db::Utils {

template <typename T>
concept IsDbModel = requires(T m) {
  T::schema();
  T::tableName;
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

#endif
