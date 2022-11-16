/// \file fon9/seed/Field.cpp
/// \author fonwinz@gmail.com
#include "fon9/seed/Field.hpp"
#include "fon9/seed/FieldTimeStamp.hpp"

namespace fon9 { namespace seed {

Field::~Field() {
}
uint32_t Field::GetAvailSize() const {
   return this->Size_;
}
FieldNumberT Field::GetNullValue() const {
   return 0;
}
OpResult Field::SetNull(const RawWr& wr) const {
   (void)wr;
   return OpResult::not_supported_null;
}
bool Field::IsNull(const RawRd& rd) const {
   (void)rd;
   return false;
}

//--------------------------------------------------------------------------//

StrView FieldTimeStamp::GetTypeId(NumOutBuf&) const {
   return StrView{"Ts"};
}
StrView FieldTimeInterval::GetTypeId(NumOutBuf&) const {
   return StrView{"Ti"};
}
StrView FieldDayTime::GetTypeId(NumOutBuf&) const {
   return StrView{"Td"};
}

} } // namespaces
