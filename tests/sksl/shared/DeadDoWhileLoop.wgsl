struct FSIn {
  @builtin(front_facing) sk_Clockwise: bool,
};
struct FSOut {
  @location(0) sk_FragColor: vec4<f32>,
};
fn main(_stageOut: ptr<function, FSOut>) {
  {
    loop {
      {
        (*_stageOut).sk_FragColor = vec4<f32>(1.0);
      }
      continuing {
        break if !(false);
      }
    }
  }
}
@fragment fn fragmentMain(_stageIn: FSIn) -> FSOut {
  var _stageOut: FSOut;
  main(&_stageOut);
  return _stageOut;
}
