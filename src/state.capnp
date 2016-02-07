@0xbbb3eaa38ab7c4c4;

using Cxx = import "/capnp/c++.capnp";
$Cxx.namespace("proto");

using import "common.capnp".Channel;

struct State {
  channels @0 :List(Channel);
  name @1 :Text;

  defaultLevels :union {
    previous @2 :Void;
    constant @3 :List(UInt16);
  }

  levels @4 :List(UInt16);
}
