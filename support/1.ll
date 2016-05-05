; ModuleID = 'support/1.bc'
target datalayout = "e-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-pc-linux-gnu"

; Function Attrs: nounwind uwtable
define void @function_1(i32 %x) #0 {
  %1 = alloca i32, align 4
  store i32 %x, i32* %1, align 4
  br label %2

; <label>:2                                       ; preds = %16, %0
  %3 = load i32* %1, align 4
  %4 = icmp ugt i32 %3, 0
  br i1 %4, label %6, label %5

; <label>:5                                       ; preds = %2
  br label %17

; <label>:6                                       ; preds = %2
  %7 = load i32* %1, align 4
  %8 = urem i32 %7, 4
  %9 = icmp eq i32 %8, 0
  br i1 %9, label %10, label %13

; <label>:10                                      ; preds = %6
  %11 = load i32* %1, align 4
  %12 = add i32 %11, -1
  store i32 %12, i32* %1, align 4
  br label %16

; <label>:13                                      ; preds = %6
  %14 = load i32* %1, align 4
  %15 = add i32 %14, -1
  store i32 %15, i32* %1, align 4
  br label %16

; <label>:16                                      ; preds = %13, %10
  br label %2

; <label>:17                                      ; preds = %5
  ret void
}

; Function Attrs: nounwind uwtable
define i32 @main() #0 {
  %1 = alloca i32, align 4
  store i32 0, i32* %1
  call void @function_1(i32 100)
  ret i32 0
}

attributes #0 = { nounwind uwtable "less-precise-fpmad"="false" "no-frame-pointer-elim"="true" "no-frame-pointer-elim-non-leaf" "no-infs-fp-math"="false" "no-nans-fp-math"="false" "stack-protector-buffer-size"="8" "unsafe-fp-math"="false" "use-soft-float"="false" }

!llvm.ident = !{!0}

!0 = !{!"Ubuntu clang version 3.4-1ubuntu3~precise2 (tags/RELEASE_34/final) (based on LLVM 3.4)"}
