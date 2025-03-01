struct FSIn {
  @builtin(front_facing) sk_Clockwise: bool,
  @builtin(position) sk_FragCoord: vec4<f32>,
};
struct FSOut {
  @location(0) sk_FragColor: vec4<f32>,
};
struct _GlobalUniforms {
  colorGreen: vec4<f32>,
  colorRed: vec4<f32>,
  testMatrix3x3: mat3x3<f32>,
  testMatrix4x4: mat4x4<f32>,
};
@binding(0) @group(0) var<uniform> _globalUniforms: _GlobalUniforms;
fn test3x3_b() -> bool {
  {
    var expected: vec3<f32> = vec3<f32>(3.0, 2.0, 1.0);
    var vec: vec3<f32>;
    {
      var c: i32 = 0;
      loop {
        if c < 3 {
          {
            {
              var r: i32 = 0;
              loop {
                if r < 3 {
                  {
                    let _skTemp0 = vec3<i32>(2, 1, 0)[r];
                    vec[_skTemp0] = _globalUniforms.testMatrix3x3[c][r];
                  }
                } else {
                  break;
                }
                continuing {
                  r = r + i32(1);
                }
              }
            }
            if (any(vec != expected)) {
              {
                return false;
              }
            }
            expected = expected + 3.0;
          }
        } else {
          break;
        }
        continuing {
          c = c + i32(1);
        }
      }
    }
    return true;
  }
}
fn test4x4_b() -> bool {
  {
    var expected: vec4<f32> = vec4<f32>(4.0, 3.0, 2.0, 1.0);
    var vec: vec4<f32>;
    {
      var c: i32 = 0;
      loop {
        if c < 4 {
          {
            {
              var r: i32 = 0;
              loop {
                if r < 4 {
                  {
                    let _skTemp1 = vec4<i32>(3, 2, 1, 0)[r];
                    vec[_skTemp1] = _globalUniforms.testMatrix4x4[c][r];
                  }
                } else {
                  break;
                }
                continuing {
                  r = r + i32(1);
                }
              }
            }
            if (any(vec != expected)) {
              {
                return false;
              }
            }
            expected = expected + 4.0;
          }
        } else {
          break;
        }
        continuing {
          c = c + i32(1);
        }
      }
    }
    return true;
  }
}
fn main(coords: vec2<f32>) -> vec4<f32> {
  {
    var _skTemp2: vec4<f32>;
    var _skTemp3: bool;
    let _skTemp4 = test3x3_b();
    if _skTemp4 {
      let _skTemp5 = test4x4_b();
      _skTemp3 = _skTemp5;
    } else {
      _skTemp3 = false;
    }
    if _skTemp3 {
      _skTemp2 = _globalUniforms.colorGreen;
    } else {
      _skTemp2 = _globalUniforms.colorRed;
    }
    return _skTemp2;
  }
}
@fragment fn fragmentMain(_stageIn: FSIn) -> FSOut {
  var _stageOut: FSOut;
  _stageOut.sk_FragColor = main(_stageIn.sk_FragCoord.xy);
  return _stageOut;
}
