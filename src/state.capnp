@0xbbb3eaa38ab7c4c4;

using Cxx = import "/capnp/c++.capnp";
$Cxx.namespace("proto");

struct Params {
  channels @0 :List(Float32);
  name @1 :Text;
  defaultLevels @2 :List(UInt16);
  levels @3 :List(UInt16);
}
