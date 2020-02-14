/// \file fon9/seed/PodOp.cpp
/// \author fonwinz@gmail.com
#include "fon9/seed/PodOp.hpp"
#include "fon9/seed/Tree.hpp"
#include "fon9/RevPut.hpp"

namespace fon9 { namespace seed {

PodOp::~PodOp() {
}
TreeSP PodOp::MakeSapling(Tab& tab) {
   return this->GetSapling(tab);
}
TreeSP PodOp::GetSapling(Tab& /*tab*/) {
   return TreeSP{};
}

//--------------------------------------------------------------------------//

void PodOpDefault::OnSeedCommand(Tab* tab, StrView cmdln, FnCommandResultHandler resHandler) {
   this->OnSeedCommand_NotSupported(tab, cmdln, std::move(resHandler));
}
void PodOpDefault::OnSeedCommand_NotSupported(Tab* tab, StrView cmdln, FnCommandResultHandler&& resHandler) {
   (void)cmdln;
   this->OpResult_ = OpResult::not_supported_cmd;
   this->Tab_ = tab;
   resHandler(*this, nullptr);
}

void PodOpDefault::BeginRead(Tab& tab, FnReadOp fnCallback) {
   this->BeginRead_NotSupported(tab, std::move(fnCallback));
}
void PodOpDefault::BeginRead_NotSupported(Tab& tab, FnReadOp&& fnCallback) {
   this->OpResult_ = OpResult::not_supported_read;
   this->Tab_ = &tab;
   fnCallback(*this, nullptr);
}

void PodOpDefault::BeginWrite(Tab& tab, FnWriteOp fnCallback) {
   this->BeginWrite_NotSupported(tab, std::move(fnCallback));
}
void PodOpDefault::BeginWrite_NotSupported(Tab& tab, FnWriteOp&& fnCallback) {
   this->OpResult_ = OpResult::not_supported_write;
   this->Tab_ = &tab;
   fnCallback(*this, nullptr);
}

//--------------------------------------------------------------------------//

fon9_API void FieldsCellRevPrint0NoSpl(const Fields& fields, const RawRd& rd, RevBuffer& rbuf, char chSplitter) {
   if (size_t fldidx = fields.size()) {
      while (const Field* fld = fields.Get(--fldidx)) {
         fld->CellRevPrint(rd, nullptr, rbuf);
         if (fldidx == 0)
            break;
         RevPutChar(rbuf, chSplitter);
      }
   }
}
fon9_API void FieldsCellRevPrint(const Fields& fields, const RawRd& rd, RevBuffer& rbuf, char chSplitter) {
   if (size_t fldidx = fields.size()) {
      while (const Field* fld = fields.Get(--fldidx)) {
         fld->CellRevPrint(rd, nullptr, rbuf);
         RevPutChar(rbuf, chSplitter);
      }
   }
}
fon9_API void FieldsNameRevPrint(const Layout* layout, const Tab& tab, RevBuffer& rbuf, char chSplitter) {
   const Fields& flds = tab.Fields_;
   if (size_t fldidx = flds.size()) {
      RevPrint(rbuf, flds.Get(--fldidx)->Name_);
      while (fldidx > 0) {
         RevPrint(rbuf, flds.Get(--fldidx)->Name_, chSplitter);
      }
      if (layout)
         RevPrint(rbuf, chSplitter);
   }
   if (layout)
      RevPrint(rbuf, layout->KeyField_->Name_);
}

} } // namespaces
