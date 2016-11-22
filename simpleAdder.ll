; ModuleID = 'phigep.c'
target datalayout = "e-p:64:64:64-i1:8:8-i8:8:8-i16:16:16-i32:32:32-i64:64:64-f32:32:32-f64:64:64-v64:64:64-v128:128:128-a0:0:64-s0:64:64-f80:128:128-n8:16:32:64-S128"
target triple = "x86_64-pc-linux-gnu"

@.str = private unnamed_addr constant [8 x i8] c"enter n\00", align 1
@.str1 = private unnamed_addr constant [3 x i8] c"%d\00", align 1
@.str2 = private unnamed_addr constant [9 x i8] c"sum = %d\00", align 1

; Function Attrs: nounwind uwtable
define i32 @main() #0 {
  %data = alloca [1000 x i32], align 16
  %n = alloca i32, align 4
  %1 = bitcast [1000 x i32]* %data to i8*
  call void @llvm.lifetime.start(i64 4000, i8* %1) #1
  %2 = call i32 (i8*, ...)* @printf(i8* getelementptr inbounds ([8 x i8]* @.str, i64 0, i64 0)) #1
  %3 = call i32 (i8*, ...)* @__isoc99_scanf(i8* getelementptr inbounds ([3 x i8]* @.str1, i64 0, i64 0), i32* %n) #1
  %4 = load i32* %n, align 4, !tbaa !1
  %5 = icmp sgt i32 %4, 0
  br i1 %5, label %.lr.ph, label %._crit_edge

.lr.ph:                                           ; preds = %0
  %6 = load i32* %n, align 4, !tbaa !1
  br label %7

; <label>:7                                       ; preds = %.lr.ph, %17
  %indvars.iv = phi i64 [ 0, %.lr.ph ], [ %indvars.iv.next, %17 ]
  %sum.02 = phi i32 [ 0, %.lr.ph ], [ %20, %17 ]
  %8 = and i64 %indvars.iv, 1
  %9 = icmp eq i64 %8, 0
  br i1 %9, label %10, label %13

; <label>:10                                      ; preds = %7
  %11 = getelementptr inbounds [1000 x i32]* %data, i64 0, i64 %indvars.iv
  %12 = trunc i64 %indvars.iv to i32
  store i32 %12, i32* %11, align 4, !tbaa !1
  br label %17

; <label>:13                                      ; preds = %7
  %14 = trunc i64 %indvars.iv to i32
  %15 = shl nsw i32 %14, 1
  %16 = getelementptr inbounds [1000 x i32]* %data, i64 0, i64 %indvars.iv
  store i32 %15, i32* %16, align 4, !tbaa !1
  br label %17

; <label>:17                                      ; preds = %13, %10
  %18 = getelementptr inbounds [1000 x i32]* %data, i64 0, i64 %indvars.iv
  %19 = load i32* %18, align 4, !tbaa !1
  %20 = add nsw i32 %19, %sum.02
  %indvars.iv.next = add nuw nsw i64 %indvars.iv, 1
  %21 = trunc i64 %indvars.iv.next to i32
  %22 = icmp slt i32 %21, %6
  br i1 %22, label %7, label %._crit_edge

._crit_edge:                                      ; preds = %17, %0
  %sum.0.lcssa = phi i32 [ 0, %0 ], [ %20, %17 ]
  %23 = call i32 (i8*, ...)* @printf(i8* getelementptr inbounds ([9 x i8]* @.str2, i64 0, i64 0), i32 %sum.0.lcssa) #1
  call void @llvm.lifetime.end(i64 4000, i8* %1) #1
  ret i32 0
}

; Function Attrs: nounwind
declare void @llvm.lifetime.start(i64, i8* nocapture) #1

; Function Attrs: nounwind
declare i32 @printf(i8* nocapture readonly, ...) #2

; Function Attrs: nounwind
declare i32 @__isoc99_scanf(i8* nocapture readonly, ...) #2

; Function Attrs: nounwind
declare void @llvm.lifetime.end(i64, i8* nocapture) #1

attributes #0 = { nounwind uwtable "less-precise-fpmad"="false" "no-frame-pointer-elim"="false" "no-infs-fp-math"="false" "no-nans-fp-math"="false" "stack-protector-buffer-size"="8" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #1 = { nounwind }
attributes #2 = { nounwind "less-precise-fpmad"="false" "no-frame-pointer-elim"="false" "no-infs-fp-math"="false" "no-nans-fp-math"="false" "stack-protector-buffer-size"="8" "unsafe-fp-math"="false" "use-soft-float"="false" }

!llvm.ident = !{!0}

!0 = metadata !{metadata !"Ubuntu clang version 3.4-1ubuntu3 (tags/RELEASE_34/final) (based on LLVM 3.4)"}
!1 = metadata !{metadata !2, metadata !2, i64 0}
!2 = metadata !{metadata !"int", metadata !3, i64 0}
!3 = metadata !{metadata !"omnipotent char", metadata !4, i64 0}
!4 = metadata !{metadata !"Simple C/C++ TBAA"}
