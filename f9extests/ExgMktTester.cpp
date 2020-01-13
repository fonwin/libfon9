// \file f9extests/ExgMktTester.cpp
// \author fonwinz@gmail.com
#define _CRT_SECURE_NO_WARNINGS
#include "f9extests/ExgMktTester.hpp"
#include <iostream>

namespace f9extests {

MktDataFile::MktDataFile(char* argv[]) : Size_{0}, Buffer_{nullptr} {
   std::cout << "MktDataFile=" << argv[1] << std::endl;
   FILE* fd = fopen(argv[1], "rb");
   if (!fd) {
      perror("Open MktDataFile fail");
      return;
   }
   const std::unique_ptr<FILE, decltype(&fclose)> fdauto{fd, &fclose};
   fseek(fd, 0, SEEK_END);
   auto fsize = ftell(fd);
   std::cout << "FileSize=" << fsize << std::endl;

   size_t rdfrom = StrToVal(argv[2]);
   std::cout << "ReadFrom=" << rdfrom << std::endl;
   if (rdfrom >= static_cast<size_t>(fsize)) {
      std::cout << "ReadFrom >= FileSize" << std::endl;
      return;
   }
   size_t rdsz = StrToVal(argv[3]);
   if (rdsz <= 0)
      rdsz = fsize - rdfrom;
   else if (rdfrom + rdsz > static_cast<size_t>(fsize)) {
      std::cout << "ReadFrom + ReadSize > FileSize" << std::endl;
      return;
   }
   std::cout << "ReadSize=" << rdsz << std::endl;
   *const_cast<size_t*>(&this->Size_) = rdsz;
   *const_cast<char**>(&this->Buffer_) = reinterpret_cast<char*>(malloc(rdsz));
   if (this->Buffer_ == nullptr) {
      std::cout << "err=Alloc buffer for read." << std::endl;
      return;
   }
   fseek(fd, static_cast<long>(rdfrom), SEEK_SET);
   size_t r = fread(this->Buffer_, 1, rdsz, fd);
   if (r != rdsz) {
      std::cout << "err=Read " << r << " bytes, but expect=" << rdsz << std::endl;
      return;
   }
}
MktDataFile::~MktDataFile() {
   free(this->Buffer_);
}

} // namespaces
