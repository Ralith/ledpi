@0xc24f4c25470b59f4;

struct Channel {
  name @0 :Text;

  gpio @1 :UInt16;
  # pin

  spectra @2 :List(Float32);
  # 60 5-nm buckets from 400 to 700
}
