#pragma once

#include "behaviorTreeData.hpp"

///
/// Super annoying problem with initializer lists is that they copy all the time, so 
/// you can't normally aggregate initialize a class that has a collection of non-copyable objects
/// unless you wrap the construction of it.
///
/// from https://stackoverflow.com/questions/9618268/initializing-container-of-unique-ptrs-from-initializer-list-fails-with-gcc-4-7
/// 
#include<memory>
#include<vector>
#include<type_traits>

template <class T> auto move_to_unique(T&& t) {
  return std::make_unique<std::remove_reference_t<T>>(std::move(t));
}
template <class V, class ... Args> auto make_vector_unique(Args ... args) {
  std::vector<std::unique_ptr<V>> rv;
  (rv.push_back(move_to_unique(args)), ...);
  return rv;
}
////

FuncString getFuncName(const StackNode& stackNode) {
  FuncString ret;
  if (stackNode.blockText.objectId == "0x0") {
    return ret;
  }

  ret.valid = true;
  const std::string& str = stackNode.blockText.functionName;

  /*
  references:

  pretty print of the constructor of a nested "NestedGuy" struct defined in the constructor of struct "TestStruct"
  [TestProcess.cpp]::[TestStruct::TestStruct(std::function<int (int)>, int (*)(int, int))::NestedGuy::NestedGuy()]

  pretty print of an anonymous (lambda) function defined/called by a constructor
  [TestProcess.cpp]::[auto TestStruct::TestStruct(std::function<int (int)>, int (*)(int, int))::(anonymous class)::operator()(int) const]

  Thus, to get the "name part" of the function, you need to start from the closing parens and count parens in reverse until you reach the
  the opening parents.  Then copy everything until you reach either "::" or "[" or " "
  
  Additionally, we're only interested in opening lines, not closing lines (or we end up with dupes for everything).
  */
  
  int count = 1; 
  if (size_t argsParensEnd = str.rfind(")"); argsParensEnd != std::string::npos) {
    for (; (count > 0) && (argsParensEnd > 0); --argsParensEnd) {
      switch(str[argsParensEnd - 1]) {
        case '(':
          --count;
          break;
        case ')':
          ++count;
          break;
        default:
          break;
      }
    }

    // funcNameEnd will either by 0 or point to opening parens "(" which immediately
    // follows the function name
    size_t funcNameEnd = argsParensEnd;
    if (size_t funcNameStart = str.rfind("::", funcNameEnd);
        funcNameStart != std::string::npos) {
      ret.name = str.substr(funcNameStart + 2, funcNameEnd - funcNameStart - 2);
      size_t classNameStart = str.rfind("::", funcNameStart - 1);
      if (classNameStart == std::string::npos) {
        classNameStart = 0;
      } else {
        classNameStart += 2;
      }
      ret.className = str.substr(classNameStart, funcNameStart - classNameStart);
      // printf("funcNameStart: [%zu] | funcNameEnd: [%zu] | funcNameChars: [%zu] | classNameStart: [%zu] | classNameChars: [%zu] \n", funcNameStart, funcNameEnd, funcNameEnd - funcNameStart - 2, classNameStart, funcNameStart - classNameStart);  
      // printf("func name stuff: funcName: [%s], className: [%s]\n", ret.name.c_str(), ret.className.c_str());
    }

    if (stackNode.logLineType == CapLogType::BLOCK_SCOPE_OPEN) {
      if (ret.name.find("~") == 0) {
        ret.isDtor = true;
      } else if (ret.name == ret.className) {
        ret.isCtor = true;
      }
    }
  }

  return ret;
}

// Scanned lines are in this format:
// Label | key: [value] | key2: [value2] | etc.
ScanLine parseScanLine(const StackNode& stackNode) {
  ScanLine ret;
  if (stackNode.logLineType != CapLogType::BLOCK_INNER_LINE) {
    return ret;
  }

  ret.valid = true;
  const std::string& str = stackNode.messageText.innerPayload;

  // first character is always a space, and everything before first pipe char is assumed to be the label (or entire line if no pipe)
  size_t nextTokenSet = str.find(" | ");
  // skip first character, which is a space.
  size_t substringStart = 1;
  size_t substringEnd = (nextTokenSet != std::string::npos) ? nextTokenSet : str.size() - 1;
  ret.label = str.substr(substringStart, substringEnd - substringStart);
  // printf("label: [%s]\n", ret.label.c_str());

  while (nextTokenSet != std::string::npos) {
    nextTokenSet += 3;
    size_t substringStart = nextTokenSet;
    nextTokenSet = str.find(" | ", nextTokenSet);
    size_t substringEnd = (nextTokenSet != std::string::npos) ? nextTokenSet : str.size() - 1;
    // printf("substringStart: [%zu] | substringEnd: [%zu]\n", substringStart, substringEnd);

    size_t valueStart = str.find("[", substringStart);
    size_t valueEnd = str.rfind("]", substringEnd);
    
    KVP kvp;

    if (valueStart != std::string::npos && valueEnd != std::string::npos) {
      kvp.key = str.substr(substringStart, valueStart - substringStart);
      kvp.value = str.substr(valueStart + 1, valueEnd - valueStart - 1);
      // printf("key: [%s], value: [%s]\n", kvp.key.c_str(), kvp.value.c_str());
    } else {
      kvp.key = str.substr(substringStart, substringEnd - substringStart);
      // printf("key: [%s]\n", kvp.key.c_str());
    }

    ret.kvps.push_back(std::move(kvp));
  }

  return ret;
}