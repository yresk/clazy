/*
   This file is part of the clazy static checker.

  Copyright (C) 2015 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com
  Author: Sérgio Martins <sergio.martins@kdab.com>

  Copyright (C) 2015 Sergio Martins <smartins@kde.org>

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Library General Public
  License as published by the Free Software Foundation; either
  version 2 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Library General Public License for more details.

  You should have received a copy of the GNU Library General Public License
  along with this library; see the file COPYING.LIB.  If not, write to
  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
  Boston, MA 02110-1301, USA.
*/

#include "qgetenv.h"
#include "Utils.h"
#include "checkmanager.h"
#include "StringUtils.h"
#include "FixItUtils.h"
#include <clang/AST/AST.h>
#include <clang/Lex/Lexer.h>

using namespace clang;
using namespace std;

enum Fixit {
    FixitNone = 0,
    FixitAll = 0x1 // More granularity isn't needed I guess
};

QGetEnv::QGetEnv(const std::string &name, const clang::CompilerInstance &ci)
    : CheckBase(name, ci)
{

}



void QGetEnv::VisitStmt(clang::Stmt *stmt)
{
    // Lets check only in function calls. Otherwise there are too many false positives, it's common
    // to implicit cast to bool when checking pointers for validity, like if (ptr)

    CXXMemberCallExpr *memberCall = dyn_cast<CXXMemberCallExpr>(stmt);
    if (!memberCall)
        return;

    CXXMethodDecl *method = memberCall->getMethodDecl();
    if (!method)
        return;

    CXXRecordDecl *record = method->getParent();
    if (!record || record->getNameAsString() != "QByteArray") {
        return;
    }

    std::vector<CallExpr *> calls = Utils::callListForChain(memberCall);
    if (calls.size() != 2)
        return;

    CallExpr *qgetEnvCall = calls.back();

    FunctionDecl *func = qgetEnvCall->getDirectCallee();

    if (!func || func->getNameAsString() != "qgetenv")
        return;

    string methodname = method->getNameAsString();
    string errorMsg;
    std::string replacement;
    if (methodname == "isEmpty") {
        errorMsg = "qgetenv().isEmpty() allocates.";
        replacement = "qEnvironmentVariableIsEmpty";
    } else if (methodname == "isNull") {
        errorMsg = "qgetenv().isNull() allocates.";
        replacement = "qEnvironmentVariableIsSet";
    } else if (methodname == "toInt") {
        errorMsg = "qgetenv().toInt() is slow.";
        replacement = "qEnvironmentVariableIntValue";
    }

    if (!errorMsg.empty()) {
        std::vector<FixItHint> fixits;
        if (isFixitEnabled(FixitAll)) {
            const bool success = FixItUtils::transformTwoCallsIntoOne(m_ci, qgetEnvCall, memberCall, replacement, fixits);
            if (!success) {
                queueManualFixitWarning(memberCall->getLocStart(), FixitAll);
            }
        }

        errorMsg += " Use " + replacement + "() instead";
        emitWarning(memberCall->getLocStart(), errorMsg.c_str(), fixits);
    }
}


const char *const s_checkName = "qgetenv";
REGISTER_CHECK_WITH_FLAGS(s_checkName, QGetEnv, CheckLevel0)
REGISTER_FIXIT(FixitAll, "fix-qgetenv", s_checkName)
