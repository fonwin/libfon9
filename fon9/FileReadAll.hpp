/// \file fon9/FileReadAll.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_FileReadAll_hpp__
#define __fon9_FileReadAll_hpp__
#include "fon9/buffer/DcQueueList.hpp"
#include "fon9/buffer/FwdBufferList.hpp"
#include "fon9/File.hpp"

namespace fon9 {

using ReadAllResult = Result2T<ErrC>;

/// fname.empty() 則不需要開啟檔案;
fon9_API ReadAllResult OpenReadAll(File& fd, CharVector& fbuf, std::string fname, FileMode fmode);
   
template <class FileT, typename PosT, class ConsumerT>
typename FileT::Result FileReadAll(FileT& fd, PosT& fpos, ConsumerT&& consumer) {
   DcQueueList rdbuf;
   for (;;) {
      FwdBufferNode* node = FwdBufferNode::Alloc(32 * 1024);
      auto res = fd.Read(fpos, node->GetDataEnd(), node->GetRemainSize());
      if (res.IsError()) {
         FreeNode(node);
         return res;
      }
      if (res.GetResult() <= 0) {
         FreeNode(node);
         return res;
      }
      fpos += res.GetResult();
      node->SetDataEnd(node->GetDataEnd() + res.GetResult());
      rdbuf.push_back(node);
      if (!consumer(rdbuf, res))
         return res;
   }
}

} // namespaces
#endif//__fon9_FileReadAll_hpp__
