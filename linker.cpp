#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <cctype>
#include <climits>
#include <cstdint>
#include <cassert>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <vector>
#include <algorithm>
#include <optional>
#include <memory>
#include <stdexcept>
#include <set>
#include <map>
#include <queue>
#include <deque>
#include <stack>
#include <unordered_map>
#include <unordered_set>

class FuncAttributes {
  public:
    std::string definition;
    std::map<size_t, std::string> imports; // all imports with position from the function start
};

class CLinker {
  private:
    std::map<std::string, FuncAttributes> funcs;

    unsigned readNum(std::ifstream &file) const {
      unsigned ret = 0;
        if (!file.read((char*)&ret, sizeof(unsigned)))
          throw std::runtime_error("Failed to load enough data");
      return ret;
    }

    std::string readString(std::ifstream &file, size_t len) const {
      std::string ret;
      ret.reserve(len);
      for (size_t i = 0; i < len; i++) {
        char c;
        if (!file.get(c))
          throw std::runtime_error("Failed to load enough data");
        ret.push_back(c);
      }
      return ret;
    }

  public:
    CLinker& addFile (const std::string &fileName) {
      std::ifstream file(fileName, std::ios::binary);
      if (file.fail()) throw std::runtime_error ("Failed to read input file");

      // Parsing header:
      unsigned exportNum = readNum(file);
      unsigned importNum = readNum(file);
      unsigned bytes = readNum(file);
      if (exportNum == 0) return *this;

      // Maps absolute func start positions to the func name
      // will be used in exports for getting import name
      std::map<size_t, std::string> exportPositions;  

      // Parsing exports:
      for (unsigned i = 0; i < exportNum; i++) {
        unsigned char nameLen;
        if (!file.read((char*)&nameLen, 1))
          throw std::runtime_error("Incorrect name length in exports");

        std::string name = readString(file, (size_t)nameLen);
        if (funcs.contains(name))
          throw std::runtime_error("Duplicate symbol");
        unsigned offset = readNum(file);
        if (exportPositions.contains(offset)) // ensures offset uniqueness
          throw std::runtime_error("Duplicate export offset");

        FuncAttributes func;
        funcs[name] = func;
        exportPositions[offset] = name;
      }

      // Parsing imports:
      for (unsigned i = 0; i < importNum; i++) {
        unsigned char nameLen;
        if (!file.read((char*)&nameLen, 1))
          throw std::runtime_error("Incorrect name length in imports");

        std::string name = readString(file, (size_t)nameLen);
        unsigned usageCount = readNum(file);

        // save all the import locations in a exported function to it's FuncAttributes
        for (unsigned i = 0; i < usageCount; i++) {
          unsigned addr = readNum(file);
          auto importedToIter = exportPositions.upper_bound(addr);
          if (importedToIter != exportPositions.begin()) importedToIter--;

          size_t funcPos = importedToIter->first;
          std::string funcName = importedToIter->second;
          std::string importedTo = funcName;
          funcs[importedTo].imports[addr - funcPos] = name;

        }
      }

      // Parsing definitions:
      for (auto it = exportPositions.begin(); it != exportPositions.end();) {
        auto curr = it;
        it++; // for looking at the next definition (to know how long is the current func definition)
        size_t currPos = curr->first;
        size_t nextPos = it->first;

        // if this is the last definition in the object file, then the end is the end of file:
        if (bytes < currPos)
          throw std::runtime_error("Incorrect byte length from the header");
        size_t codeLength = it == exportPositions.end() ?
          bytes - currPos : nextPos - currPos;

        std::string funcName = curr->second;
        funcs[funcName].definition = readString(file, codeLength);
      }
      file.close();
      return *this;
    }
    
  private:
    // finding recursively all used functions in this current linking process using DFS
    // and placing them in std::set used
    void findUsed (std::set<std::string>& used, const std::string& currFunc) {
      if (!funcs.contains(currFunc)) 
        throw std::runtime_error("Missing function definition");
      if (used.contains(currFunc)) return;
      used.insert(currFunc);
      for (const auto& [relativeAddr, importedFuncName] : funcs[currFunc].imports) {
        findUsed(used, importedFuncName);
      }
    }

    // given an output std::string with raw function definitions,
    // this function writes all import addreses of one specific linked function
    void writeAddresses (std::string& output, std::map<std::string, size_t>& outputPositions,
                        const std::string& funcName, size_t funcPos) {
      for (const auto& [relativeAddr, importedFuncName] : funcs[funcName].imports) {
        unsigned char addr[4];
        // Make sure addresses are in little endian byte order:
        for (int i = 0; i < 4; i++)
          addr[i] = (outputPositions[importedFuncName] >> 8 * i) & 0xFF;
        memcpy(&output[funcPos + relativeAddr], addr, 4);
      }
    }

  public:
    void linkOutput (const std::string &fileName, const std::string &entryPoint) {
      if (!funcs.contains(entryPoint)) 
        throw std::runtime_error("Entry function has not yet been exported");

      std::string output = funcs[entryPoint].definition;
      std::set<std::string> used;
      findUsed(used, entryPoint);
      used.erase(entryPoint); // so that entryPoint is not added twice

      // paste all raw definitions into the output
      std::map<std::string, size_t> outputPositions = { {entryPoint, 0} };
      for (const auto& funcName : used) {
        outputPositions[funcName] = output.size();
        output += funcs[funcName].definition;
      }

      // Adding function references
      for (const auto& [funcName, absAddress] : outputPositions) {
        writeAddresses(output, outputPositions, funcName, absAddress);
      }

      std::ofstream ofs(fileName, std::ios::binary);
      if (!ofs) throw std::runtime_error("Output file couldn't be opened");
      ofs.write(output.data(), output.size());
      if (!ofs) throw std::runtime_error("Data couldn't be written to output file");
      ofs.close();
    }
};

int main ()
{
  CLinker () . addFile ( "0in0.o" ) . linkOutput ( "0out", "main" );
  CLinker () . addFile ( "0in0.o" ) . linkOutput ( "0out", "towersOfHanoi" );
  CLinker().addFile("0in0.o").addFile("0in1_my.o").addFile("0in2_my.o").linkOutput("0out","main");

  // -- Original tests set 1: -- 
  CLinker () . addFile ( "0in0.o" ) . linkOutput ( "0out", "strlen" );

  CLinker () . addFile ( "1in0.o" ) . linkOutput ( "1out", "main" );

  CLinker () . addFile ( "2in0.o" ) . addFile ( "2in1.o" ) . linkOutput ( "2out", "main" );

  CLinker () . addFile ( "3in0.o" ) . addFile ( "3in1.o" ) . linkOutput ( "3out", "towersOfHanoi" );

  try
  {
    CLinker () . addFile ( "4in0.o" ) . addFile ( "4in1.o" ) . linkOutput ( "4out", "unusedFunc" );
    assert ( "missing an exception" == nullptr );
  }
  catch ( const std::runtime_error & e )
  {
    // e . what (): Undefined symbol qsort
  }
  catch ( ... )
  {
    assert ( "invalid exception" == nullptr );
  }

  try
  {
    CLinker () . addFile ( "5in0.o" ) . linkOutput ( "5out", "main" );
    assert ( "missing an exception" == nullptr );
  }
  catch ( const std::runtime_error & e )
  {
    // e . what (): Duplicate symbol: printf
  }
  catch ( ... )
  {
    assert ( "invalid exception" == nullptr );
  }

  try
  {
    CLinker () . addFile ( "6in0.o" ) . linkOutput ( "6out", "strlen" );
    assert ( "missing an exception" == nullptr );
  }
  catch ( const std::runtime_error & e )
  {
    // e . what (): Cannot read input file
  }
  catch ( ... )
  {
    assert ( "invalid exception" == nullptr );
  }

  try
  {
    CLinker () . addFile ( "7in0.o" ) . linkOutput ( "7out", "strlen2" );
    assert ( "missing an exception" == nullptr );
  }
  catch ( const std::runtime_error & e )
  {
    // e . what (): Undefined symbol strlen2
  }
  catch ( ... )
  {
    assert ( "invalid exception" == nullptr );
  }

  // -- Original tests set 2: -- 
  std::cout << "TEST: 0010.o\n";
  CLinker () . addFile ( "0010_0.o" ) . addFile ( "0010_1.o" ) . addFile ( "0010_2.o" ) . addFile ( "0010_3.o" ) . linkOutput ( "0010_out", "pdrolowjjgdwxiadj" );
  std::cout << "TEST: 0011.o\n";
  CLinker () . addFile ( "0011_0.o" ) . addFile ( "0011_1.o" ) . linkOutput ( "0011_out", "yntvlhvtp" );
  std::cout << "TEST: 0012.o\n";
  CLinker () . addFile ( "0012_0.o" ) . addFile ( "0012_1.o" ) . addFile ( "0012_2.o" ) . linkOutput ( "0012_out", "acnskqfuegem" );
  std::cout << "TEST: 0013.o\n";
  CLinker () . addFile ( "0013_0.o" ) . addFile ( "0013_1.o" ) . addFile ( "0013_2.o" ) . linkOutput ( "0013_out", "yvjbkannhcusuktuhl" );
  std::cout << "TEST: 0014.o\n";
  CLinker () . addFile ( "0014_0.o" ) . addFile ( "0014_1.o" ) . addFile ( "0014_2.o" ) . linkOutput ( "0014_out", "adqcwiahautvfi" );

  return EXIT_SUCCESS;
}
