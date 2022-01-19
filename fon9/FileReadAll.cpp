// \file fon9/FileReadAll.cpp
// \author fonwinz@gmail.com
#include "fon9/FileReadAll.hpp"

namespace fon9 {

fon9_API ReadAllResult OpenReadAll(File& fd, CharVector& fbuf, std::string fname, FileMode fmode) {
   File::Result fres;
   if (!fname.empty()) {
      fres = fd.Open(std::move(fname), fmode);
      if (fres.IsError())
         return ReadAllResult{"Open", fres.GetError()};
   }
   fres = fd.GetFileSize();
   if (fres.IsError())
      return ReadAllResult{"GetFileSize", fres.GetError()};
   if (fbuf.alloc(fres.GetResult()) == nullptr)
      return ReadAllResult{"Alloc", std::errc::not_enough_memory};
   fres = fd.Read(0, fbuf.begin(), fres.GetResult());
   if (fres.IsError())
      return ReadAllResult{"Read", fres.GetError()};
   return ReadAllResult{};
}

} // namespace
