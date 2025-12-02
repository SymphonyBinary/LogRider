#pragma once

#include <vector>

template<class T>
struct ScalarVal {
  using DataType = T;
  DataType data;
};

template<class T>
struct VectorVal {
  using DataType = T;
  std::vector<DataType> data;
};