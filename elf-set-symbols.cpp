/*
  Copyright (c) 2016 by Kasun Hewage
  All rights reserved.
  
  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions are met:
      * Redistributions of source code must retain the above copyright
        notice, this list of conditions and the following disclaimer.
      * Redistributions in binary form must reproduce the above copyright
        notice, this list of conditions and the following disclaimer in the
        documentation and/or other materials provided with the distribution.
      * Neither the name of the <organization> nor the
        names of its contributors may be used to endorse or promote products
        derived from this software without specific prior written permission.
  
  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
  ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
  DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> BE LIABLE FOR ANY
  DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
  (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
  ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
  SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifdef _MSC_VER
#define _SCL_SECURE_NO_WARNINGS
#define ELFIO_NO_INTTYPES
#endif

#include <cstdio>
#include <cstdlib>
#include <list>

#include <inttypes.h>
#include <iostream>
#include <getopt.h>

#include <elfio/elfio_dump.hpp>

using namespace ELFIO;

typedef struct {
  std::string sym_name;
  std::vector<uint8_t> value;
} sym_params_t;

typedef struct {
  Elf64_Addr addr;
  std::vector<uint8_t> value;
} addr_params_t;

std::list<sym_params_t*> lst_sym_params;
std::list<addr_params_t*> lst_at_addr_params;
std::string in_file = "";
std::string out_file = "";
bool dbg_enabled = false; 

#define LOG_DEBUG(fmt...)                               \
  printf("DEBUG: "fmt);

#define LOG_INFO(fmt...)                                \
  printf(""fmt);

#define LOG_WARN(fmt...)                                \
  printf("WARN: "fmt);

#define LOG_ERROR(fmt...)                               \
  fprintf(stderr, "ERROR: "fmt);

/*------------------------------------------------------------------------------------------------*/
bool SetAtAdress(elfio& elf_obj, Elf64_Addr addr, uint8_t* val, uint32_t val_size)
{

  for (int i = 0; i < elf_obj.sections.size(); i++) {
    section* curr_sec = elf_obj.sections[i];

    if (curr_sec->get_address() <= addr &&
        curr_sec->get_address() + curr_sec->get_size() > addr) {

      if (addr + val_size > curr_sec->get_address() + curr_sec->get_size()) {
        LOG_ERROR("Size of the value is greater than the section size\n");
        return false;
      }

      Elf64_Addr offset = addr - curr_sec->get_address();
      uint8_t* data = new uint8_t[curr_sec->get_size() + 1];

      LOG_DEBUG("Found address 0x%"PRIx64" in section '%s'\n", addr, curr_sec->get_name().c_str());

      memcpy(data, curr_sec->get_data(), curr_sec->get_size());
      memcpy(&data[offset], val, val_size);
      curr_sec->set_data((const char*) data, curr_sec->get_size());

      delete data;

      return true;
    }
  }

  LOG_WARN("Cannot find address 0x%"PRIx64"\n", addr);

  return false;
}

/*------------------------------------------------------------------------------------------------*/
bool SetSymbol(elfio& elf_obj, const char* sym_name, uint8_t* val, uint32_t val_size)
{

  LOG_DEBUG("Setting symbol '%s' with size %"PRIx32"\n", sym_name, val_size);

  for (int i = 0; i < elf_obj.sections.size(); i++) {
    section* curr_sec = elf_obj.sections[i];

    if (curr_sec->get_type() == SHT_SYMTAB) {
      const symbol_section_accessor symbols(elf_obj, curr_sec);

      for (unsigned int j = 0; j < symbols.get_symbols_num(); ++j) {
        std::string name;
        Elf64_Addr sym_addr = 0;
        Elf_Xword size = 0;
        uint8_t bind = 0;
        uint8_t type = 0;
        Elf_Half section_index = 0;
        uint8_t other = 0;

        symbols.get_symbol(j, name, sym_addr, size, bind, type, section_index, other);

        if (name.compare(sym_name) == 0) {

          LOG_DEBUG("Found symbol '%s' with size %"PRIx64" at address 0x%"PRIx64"\n",
                    name.c_str(), size, sym_addr);

          if (size != val_size) {
            LOG_ERROR("Size of the symbol '%s' is %"PRIx64". But given size is %"PRIx32"\n",
                      name.c_str(), size, val_size);
            return false;
          }

          return SetAtAdress(elf_obj, sym_addr, val, val_size);
        }
      }
    }
  }

  LOG_WARN("Cannot find symbol '%s'\n", sym_name);

  return false;
}

/*------------------------------------------------------------------------------------------------*/
bool SetAtAddresses(elfio& elf_obj)
{
  for (std::list<addr_params_t*>::iterator it = lst_at_addr_params.begin();
       it != lst_at_addr_params.end(); ++it) {

    uint8_t* data = new uint8_t[(*it)->value.size()];
    for (uint32_t i = 0; i < (*it)->value.size(); i++) {
      data[i] = (*it)->value[i];
    }

    if (SetAtAdress(elf_obj, (*it)->addr, data, (*it)->value.size()) != true) {
      delete data;
      return false;
    }

    delete data;
  }
  return true;
}

/*------------------------------------------------------------------------------------------------*/
bool SetSymbols(elfio& elf_obj)
{
  for (std::list<sym_params_t*>::iterator it = lst_sym_params.begin();
       it != lst_sym_params.end(); ++it) {

    uint8_t* data = new uint8_t[(*it)->value.size()];
    for (uint32_t i = 0; i < (*it)->value.size(); i++) {
      data[i] = (*it)->value[i];
    }

    if (SetSymbol(elf_obj, (*it)->sym_name.c_str(), data, (*it)->value.size()) != true) {
      delete data;
      return false;
    }

    delete data;
  }

  return true;
}

/*------------------------------------------------------------------------------------------------*/
void TrimString(std::string& string) {
  string.erase(0, string.find_first_not_of(" \n\r\t"));
  string.erase(string.find_last_not_of(" \n\r\t") + 1);
}

/*------------------------------------------------------------------------------------------------*/
uint8_t CharToInt(char input, bool &status) {
  status = true;
  if (input >= '0' && input <= '9') {
    return input - '0';
  } else if (input >= 'A' && input <= 'F') {
    return input - 'A' + 10;
  } else if (input >= 'a' && input <= 'f') {
    return input - 'a' + 10;
  } else {
    status = false;
    return 0;
  }

  status = false;
  return 0;
}

/*------------------------------------------------------------------------------------------------*/
bool HexToBytes(const std::string& hex, std::vector<uint8_t>& bytes)
{
  if (hex.length() % 2 != 0) {
    return false;
  }

  bool status = false;
  for (uint32_t i = 0; i < hex.length(); i += 2) {
    uint8_t byte = CharToInt(hex.c_str()[i], status) * 16 + CharToInt(hex.c_str()[i + 1], status);
    if (status == false) {
      return false;
    }
    bytes.push_back(byte);
  }

  return true;
}

/*------------------------------------------------------------------------------------------------*/
bool AddToSymbols(const char* optarg)
{
  std::string arg = optarg;
  std::size_t pos = arg.find("=");

  if (pos == std::string::npos || pos + 1 >= arg.length())
  {
    LOG_ERROR("Invalid symbol format %s\n", arg.c_str());
    return false;
  }

  std::string sym_name = arg.substr(0, pos);
  TrimString(sym_name);

  std::string sym_val = arg.substr(pos + 1);
  TrimString(sym_val);

  LOG_DEBUG("Symbol name '%s', value '%s'", sym_name.c_str(), sym_val.c_str());

  if (sym_name.length() == 0 || sym_val.length() == 0) {
    LOG_ERROR("Invalid symbol format %s\n", arg.c_str());
    return false;
  }

  sym_params_t* sym_params = new sym_params_t;
  sym_params->sym_name = sym_name;

  if (HexToBytes(sym_val, sym_params->value) != true) {
    LOG_ERROR("Invalid hex string in symbol value '%s'\n", sym_name.c_str());
    delete sym_params;
    return false;
  }

  lst_sym_params.push_back(sym_params);

  return true;
}

/*------------------------------------------------------------------------------------------------*/
bool AddToAddress(const char* optarg)
{
  std::string arg = optarg;
  std::size_t pos = arg.find("=");

  if (pos == std::string::npos || pos + 1 >= arg.length())
  {
    LOG_ERROR("Invalid symbol format %s\n", arg.c_str());
    return false;
  }

  std::string address = arg.substr(0, pos);
  TrimString(address);

  std::string addr_val = arg.substr(pos + 1);
  TrimString(addr_val);

  if (address.length() == 0 || addr_val.length() == 0) {
    LOG_ERROR("Invalid address format %s", arg.c_str());
    return false;
  }

  addr_params_t* addr_params = new addr_params_t;
  uint32_t ret = std::sscanf(address.c_str(), "%"PRIx64, &addr_params->addr);
  if (ret < 1) {
    LOG_ERROR("Invalid address '%s'\n", address.c_str());
    delete addr_params;
    return false;
  }

  if (HexToBytes(addr_val, addr_params->value) != true) {
    LOG_ERROR("Invalid hex string in address value '%s'\n", address.c_str());
    delete addr_params;
    return false;
  }

  lst_at_addr_params.push_back(addr_params);

  return true;
}

/*------------------------------------------------------------------------------------------------*/
void PrintHelp()
{
  printf("Usage: elf-set-symbols -i <input elf file> -o <output elf file> [OPTIONS]\n");
  printf("\n");
  printf("-i, --input                          Input ELF file\n");
  printf("-o, --output                         Output ELF file\n");
  printf("\n");
  printf("-s, --set-sym \"<name> = <value>\"     Specify a symbol and its value to be changed\n");
  printf("                                     The value should be in hexadecimal and big-endian\n");
  printf("                                     Ex: -s \"NODE_ID=0100\" -s \"DEBUG=01\"\n");
  printf("\n");
  printf("-a, --set-at \"<address> = <value>\"   Specify an address and value to be changed\n");
  printf("                                     The address should be in hexadecimal and little-endian\n");
  printf("                                     The value should be in hexadecimal and big-endian\n");
  printf("                                     Ex: -a \"0x0027FFD7=F1\" -a \"0x0027FFD4=FF\"\n");
  printf("\n");
  printf("-h, --help                           Display this help\n");

}

/*------------------------------------------------------------------------------------------------*/
bool ProcessArguments(int argc, char** argv)
{
  int opt= 0;
  int long_index = 0;

  static struct option long_options[] = {
      {"input",     required_argument, 0,  'i' },
      {"output",    required_argument, 0,  'o' },
      {"set-sym",   required_argument, 0,  's' },
      {"set-at",    required_argument, 0,  'a' },
      {"debug",     no_argument,       0,  'd' },
      {"help",      no_argument,       0,  'h' },
      {0,           0,                 0,   0  }
  };

  while ((opt = getopt_long(argc, argv, "hi:o:s:a:d", long_options, &long_index))!= -1) {
    switch (opt) {
    case 'i':
      in_file = optarg;
      break;
    case 'o':
      out_file = optarg;
      break;
    case 's':
      AddToSymbols(optarg);
      break;
    case 'a':
      AddToAddress(optarg);
      break;
    case 'd':
      dbg_enabled = true;
      break;
    case 'h':
      PrintHelp();
      exit(EXIT_SUCCESS);
    default:
      return false;
    }
  }

  if (in_file.length() == 0)
  {
    LOG_ERROR("No input file given\n");
    return false;
  }

  if (out_file.length() == 0)
  {
    LOG_ERROR("No output file given\n");
    return false;
  }

  return true;
}

/*------------------------------------------------------------------------------------------------*/
void CleanUp()
{

  std::list<sym_params_t*>::iterator symbols_it = lst_sym_params.begin();
  while (symbols_it != lst_sym_params.end()) {
    sym_params_t* item = *symbols_it;
    symbols_it = lst_sym_params.erase(symbols_it);
    delete item;
  }

  std::list<addr_params_t*>::iterator addrs_it = lst_at_addr_params.begin();
  while (addrs_it != lst_at_addr_params.end()) {
    addr_params_t* item = *addrs_it;
    addrs_it = lst_at_addr_params.erase(addrs_it);
    delete item;
  }

}

/*------------------------------------------------------------------------------------------------*/
int main(int argc, char** argv)
{

  if (ProcessArguments(argc, argv) != true) {
    exit(EXIT_FAILURE);
  }

  elfio reader;

  if (reader.load(in_file) == false) {
    LOG_ERROR("File '%s' is not found or it is not an ELF file\n", in_file.c_str());
    exit(EXIT_FAILURE);
  }

  SetAtAddresses(reader);
  SetSymbols(reader);

  reader.save(out_file);

  CleanUp();

  return 0;
}
