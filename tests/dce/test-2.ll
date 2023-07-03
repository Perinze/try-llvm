; NOTE: Assertions have been autogenerated by utils/update_test_checks.py UTC_ARGS: --version 2
; RUN: opt -load-pass-plugin ../../tasks/dead-code-elimination/build/libDeadCodeElimination.%SHLIBEXT% -passes=dead-code-elimination -S < %s | FileCheck %s

; ModuleID = 'example-2.ll'
source_filename = "example-2.ll"
target triple = "x86_64-unknown-linux-gnu"

@.str = private unnamed_addr constant [14 x i8] c"Hello world!\0A\00", align 1

define dso_local i32 @main(i32 %argc, i8** %argv) {
; CHECK-LABEL: define dso_local i32 @main
; CHECK-SAME: (i32 [[ARGC:%.*]], ptr [[ARGV:%.*]]) {
; CHECK-NEXT:  cond.end:
; CHECK-NEXT:    [[CALL:%.*]] = call i32 (ptr, ...) @printf(ptr @.str)
; CHECK-NEXT:    ret i32 0
;
entry:
  %add = add nsw i32 %argc, 42
  %mul = mul nsw i32 %add, 2
  %cmp = icmp sgt i32 %mul, 0
  br i1 %cmp, label %cond.true, label %cond.false

cond.true:                                        ; preds = %entry
  br label %cond.end

cond.false:                                       ; preds = %entry
  br label %cond.end

cond.end:                                         ; preds = %cond.false, %cond.true
  %cond = phi i32 [ %mul, %cond.true ], [ %argc, %cond.false ]
  %call = call i32 (i8*, ...) @printf(i8* getelementptr inbounds ([14 x i8], [14 x i8]* @.str, i64 0, i64 0))
  ret i32 0
}

define dso_local i32 @main2(i32 %argc, i8** %argv) {
; CHECK-LABEL: define dso_local i32 @main2
; CHECK-SAME: (i32 [[ARGC:%.*]], ptr [[ARGV:%.*]]) {
; CHECK-NEXT:  entry:
; CHECK-NEXT:    [[ADD:%.*]] = add nsw i32 [[ARGC]], 42
; CHECK-NEXT:    [[MUL:%.*]] = mul nsw i32 [[ADD]], 2
; CHECK-NEXT:    [[CMP:%.*]] = icmp sgt i32 [[MUL]], 0
; CHECK-NEXT:    br i1 [[CMP]], label [[COND_TRUE:%.*]], label [[COND_END:%.*]]
; CHECK:       cond.true:
; CHECK-NEXT:    [[UNUSED:%.*]] = call i32 (ptr, ...) @printf(ptr @.str)
; CHECK-NEXT:    br label [[COND_END]]
; CHECK:       cond.end:
; CHECK-NEXT:    [[CALL:%.*]] = call i32 (ptr, ...) @printf(ptr @.str)
; CHECK-NEXT:    ret i32 0
;
entry:
  %add = add nsw i32 %argc, 42
  %mul = mul nsw i32 %add, 2
  %cmp = icmp sgt i32 %mul, 0
  br i1 %cmp, label %cond.true, label %cond.false

cond.true:                                        ; preds = %entry
  %unused = call i32 (i8*, ...) @printf(i8* getelementptr inbounds ([14 x i8], [14 x i8]* @.str, i64 0, i64 0))
  br label %cond.end

cond.false:                                       ; preds = %entry
  br label %cond.end

cond.end:                                         ; preds = %cond.false, %cond.true
  %cond = phi i32 [ %mul, %cond.true ], [ %argc, %cond.false ]
  %call = call i32 (i8*, ...) @printf(i8* getelementptr inbounds ([14 x i8], [14 x i8]* @.str, i64 0, i64 0))
  ret i32 0
}

define dso_local i32 @chain(i32 %argc, i8** %argv) {
; CHECK-LABEL: define dso_local i32 @chain
; CHECK-SAME: (i32 [[ARGC:%.*]], ptr [[ARGV:%.*]]) {
; CHECK-NEXT:  [[LABEL:.*]]:
; CHECK-NEXT:    [[CALL:%.*]] = call i32 (ptr, ...) @printf(ptr @.str)
; CHECK-NEXT:    ret i32 0
;
entry:
  %add = add nsw i32 %argc, 42
  %mul = mul nsw i32 %add, 2
  %cmp = icmp sgt i32 %mul, 0
  br i1 %cmp, label %cond.true, label %cond.false

cond.true:                                        ; preds = %entry
  br label %cond.end

cond.false:                                       ; preds = %entry
  br label %cond.chain.1

cond.chain.1:
  br label %cond.chain.2

cond.chain.2:
  br label %cond.chain.3

cond.chain.3:
  br label %cond.chain.4

cond.chain.4:
  br label %cond.end

cond.end:                                         ; preds = %cond.false, %cond.true
  %cond = phi i32 [ %mul, %cond.true ], [ %argc, %cond.chain.4 ]
  %call = call i32 (i8*, ...) @printf(i8* getelementptr inbounds ([14 x i8], [14 x i8]* @.str, i64 0, i64 0))
  ret i32 0
}


declare dso_local i32 @printf(i8*, ...)