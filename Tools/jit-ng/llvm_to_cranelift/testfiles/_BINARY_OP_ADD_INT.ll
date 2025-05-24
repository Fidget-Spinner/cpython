source_filename = "/tmp/tmpvpdhxpdp/_BINARY_OP_SUBSCR_LIST_INT.c"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128"
target triple = "x86_64-pc-linux-gnu"

@_JIT_JUMP_TARGET = external local_unnamed_addr global i8, align 1
@_JIT_CONTINUE = external local_unnamed_addr global i8, align 1

; Function Attrs: nounwind
define dso_local ptr @_JIT_ENTRY(ptr noundef %0, ptr noundef %1, ptr noundef %2) local_unnamed_addr #0 {
  %4 = getelementptr inbounds i8, ptr %1, i64 -8
  %5 = load i64, ptr %4, align 8, !tbaa !5
  %6 = getelementptr inbounds i8, ptr %1, i64 -16
  %7 = load i64, ptr %6, align 8, !tbaa !5
  %8 = and i64 %5, -2
  %9 = inttoptr i64 %8 to ptr
  %10 = and i64 %7, -2
  %11 = inttoptr i64 %10 to ptr
  %12 = getelementptr inbounds i8, ptr %9, i64 16
  %13 = load i64, ptr %12, align 8, !tbaa !8
  %14 = and i64 %13, -5
  %15 = icmp ugt i64 %14, 8
  br i1 %15, label %60, label %16, !prof !14

16:                                               ; preds = %3
  %17 = getelementptr inbounds i8, ptr %9, i64 24
  %18 = load i32, ptr %17, align 8, !tbaa !15
  %19 = zext i32 %18 to i64
  %20 = getelementptr inbounds i8, ptr %11, i64 16
  %21 = load i64, ptr %20, align 8, !tbaa !17
  %22 = icmp sgt i64 %21, %19
  br i1 %22, label %23, label %60, !prof !19

23:                                               ; preds = %16
  %24 = getelementptr inbounds i8, ptr %11, i64 24
  %25 = load ptr, ptr %24, align 8, !tbaa !20
  %26 = getelementptr inbounds ptr, ptr %25, i64 %19
  %27 = load ptr, ptr %26, align 8, !tbaa !22
  %28 = load i32, ptr %27, align 8, !tbaa !5
  %29 = icmp sgt i32 %28, -1
  br i1 %29, label %33, label %30

30:                                               ; preds = %23
  %31 = ptrtoint ptr %27 to i64
  %32 = or i64 %31, 1
  br label %36

33:                                               ; preds = %23
  %34 = add nuw i32 %28, 1
  store i32 %34, ptr %27, align 8, !tbaa !5
  %35 = ptrtoint ptr %27 to i64
  br label %36

36:                                               ; preds = %30, %33
  %37 = phi i64 [ %32, %30 ], [ %35, %33 ]
  %38 = getelementptr inbounds i8, ptr %0, i64 64
  store ptr %1, ptr %38, align 8, !tbaa !23
  store i64 %37, ptr %6, align 8, !tbaa !5
  %39 = and i64 %7, 1
  %40 = icmp eq i64 %39, 0
  br i1 %40, label %41, label %47

41:                                               ; preds = %36
  %42 = inttoptr i64 %7 to ptr
  %43 = load i32, ptr %42, align 8, !tbaa !5
  %44 = add i32 %43, -1
  store i32 %44, ptr %42, align 8, !tbaa !5
  %45 = icmp eq i32 %44, 0
  br i1 %45, label %46, label %47

46:                                               ; preds = %41
  tail call void @_Py_Dealloc(ptr noundef nonnull %42) #2
  br label %47

47:                                               ; preds = %36, %41, %46
  store i64 1, ptr %4, align 8, !tbaa !5
  %48 = and i64 %5, 1
  %49 = icmp eq i64 %48, 0
  br i1 %49, label %50, label %56

50:                                               ; preds = %47
  %51 = inttoptr i64 %5 to ptr
  %52 = load i32, ptr %51, align 8, !tbaa !5
  %53 = add i32 %52, -1
  store i32 %53, ptr %51, align 8, !tbaa !5
  %54 = icmp eq i32 %53, 0
  br i1 %54, label %55, label %56

55:                                               ; preds = %50
  tail call void @_Py_Dealloc(ptr noundef nonnull %51) #2
  br label %56

56:                                               ; preds = %47, %50, %55
  %57 = load ptr, ptr %38, align 8, !tbaa !23
  %58 = getelementptr inbounds i8, ptr %57, i64 -8
  %59 = musttail call ptr @_JIT_CONTINUE(ptr noundef nonnull %0, ptr noundef nonnull %58, ptr noundef %2) #2
  ret ptr %59

60:                                               ; preds = %16, %3
  %61 = tail call ptr @_JIT_JUMP_TARGET(ptr noundef %0, ptr noundef nonnull %1, ptr noundef %2) #2
  ret ptr %61
}

; Function Attrs: nonlazybind
declare void @_Py_Dealloc(ptr noundef) local_unnamed_addr #1

attributes #0 = { nounwind "min-legal-vector-width"="0" "no-builtins" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #1 = { nonlazybind "no-builtins" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #2 = { nobuiltin nounwind "no-builtins" }

!llvm.module.flags = !{!0, !1, !2, !3}
!llvm.ident = !{!4}

!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{i32 8, !"PIC Level", i32 2}
!2 = !{i32 7, !"PIE Level", i32 2}
!3 = !{i32 7, !"RtLibUseGOT", i32 1}
!4 = !{!"Ubuntu clang version 19.1.7 (++20250114103320+cd708029e0b2-1~exp1~20250114103432.75)"}
!5 = !{!6, !6, i64 0}
!6 = !{!"omnipotent char", !7, i64 0}
!7 = !{!"Simple C/C++ TBAA"}
!8 = !{!9, !13, i64 16}
!9 = !{!"_longobject", !10, i64 0, !12, i64 16}
!10 = !{!"_object", !6, i64 0, !11, i64 8}
!11 = !{!"any pointer", !6, i64 0}
!12 = !{!"_PyLongValue", !13, i64 0, !6, i64 8}
!13 = !{!"long", !6, i64 0}
!14 = !{!"branch_weights", !"expected", i32 1, i32 2000}
!15 = !{!16, !16, i64 0}
!16 = !{!"int", !6, i64 0}
!17 = !{!18, !13, i64 16}
!18 = !{!"", !10, i64 0, !13, i64 16}
!19 = !{!"branch_weights", !"expected", i32 2000, i32 1}
!20 = !{!21, !11, i64 24}
!21 = !{!"", !18, i64 0, !11, i64 24, !13, i64 32}
!22 = !{!11, !11, i64 0}
!23 = !{!24, !11, i64 64}
!24 = !{!"_PyInterpreterFrame", !6, i64 0, !11, i64 8, !6, i64 16, !11, i64 24, !11, i64 32, !11, i64 40, !11, i64 48, !11, i64 56, !11, i64 64, !25, i64 72, !6, i64 74, !6, i64 75, !6, i64 80}
!25 = !{!"short", !6, i64 0}
