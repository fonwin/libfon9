/// \file fon9/seed/SeedAclTree.cpp
/// \author fonwinz@gmail.com
#include "fon9/seed/SeedAcl.hpp"
#include "fon9/seed/FieldMaker.hpp"

namespace fon9 { namespace seed {

static Fields MakeAclTreeFields() {
   Fields fields;
   fields.Add(fon9_MakeField(AccessList::value_type, second.Rights_,       "Rights"));
   fields.Add(fon9_MakeField(AccessList::value_type, second.MaxSubrCount_, "MaxSubrCount"));
   return fields;
}
static LayoutSP MakeAclTreeLayout(TabFlag tabFlags, TreeFlag treeFlags) {
   return LayoutSP{new Layout1(fon9_MakeField(AccessList::value_type, first, "Path"),
                               new Tab{Named{"AclRights"}, MakeAclTreeFields(), tabFlags},
                               treeFlags)};
}
fon9_API LayoutSP MakeAclTreeLayout() {
   static LayoutSP aclLayout{MakeAclTreeLayout(TabFlag::NoSapling, TreeFlag{})};
   return aclLayout;
}
fon9_API LayoutSP MakeAclTreeLayoutWritable() {
   static LayoutSP aclLayout{MakeAclTreeLayout(TabFlag::NoSapling | TabFlag::Writable, TreeFlag::AddableRemovable)};
   return aclLayout;
}

} } // namespaces
