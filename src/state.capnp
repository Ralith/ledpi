@0xbbb3eaa38ab7c4c4;

using Cxx = import "/capnp/c++.capnp";
$Cxx.namespace("proto");

struct Channel {
  name @0 :Text;

  gpio @1 :UInt16;
  # pin

  spectra @2 :List(Float32);
  # 60 5-nm buckets from 400 to 700
}

struct State {
  channels @0 :List(Channel);
  name @1 :Text;

  defaultLevels :union {
    previous @2 :Void;
    constant @3 :List(UInt16);
  }

  levels @4 :List(UInt16);
}
