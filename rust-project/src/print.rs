pub fn print_func() {
    // println!() 带换行
    // print!() 不带换行
    print!("{{}}\n");
    println!("{{}}");
}

pub fn variable_func() {
    /*
    数字字面量          示例
    十进制              98_222
    十六进制            0xff
    八进制              0o77
    二进制              0b1111_0000
    字节 (仅限于 u8)    b'A'

    长度     有符号类型    无符号类型
    8 位     i8           u8
    16 位    i16          u16
    32 位    i32          u32
    64 位    i64          u64
    128 位   i128         u128
    arch    isize         usize

    let y: f32 = 3.0; // float
    let y: f64 = 3.0; // double default
    */
    const CONST_VALUE: u32 = 1; // 常量
    print!("{} is {}\n", stringify!(CONST_VALUE), CONST_VALUE);
    print!("{0} is {1}, {0} again is {1}\n", stringify!(CONST_VALUE), CONST_VALUE);

    let immultable_value: u32 = 1; // 不可变变量
    // let immultable_value : u32; immultable_value = 1// 不可变变量
    print!("{} is {}\n", stringify!(immultable_value), immultable_value);
    print!(
        "{0} is {1}, {0} again is {1}\n",
        stringify!(immultable_value),
        immultable_value
    );

    let mut multable_value: u32 = 1; // 可变变量
    println!("before {} is {}", stringify!(multable_value), multable_value);
    println!(
        "before {0} is {1}, {0} again is {1}",
        stringify!(multable_value),
        multable_value
    );
    multable_value = 2;
    println!("after {} is {}", stringify!(multable_value), multable_value);
    println!(
        "after {0} is {1}, {0} again is {1}",
        stringify!(multable_value),
        multable_value
    );
}
