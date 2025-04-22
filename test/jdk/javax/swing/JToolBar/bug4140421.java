/*
<<<<<<<< HEAD:test/hotspot/jtreg/runtime/interpreter/LastJsr.jasm
 * Copyright (c) 2024, Oracle and/or its affiliates. All rights reserved.
========
 * Copyright (c) 1999, 2023, Oracle and/or its affiliates. All rights reserved.
>>>>>>>> jdk-21.0.7+6:test/jdk/javax/swing/JToolBar/bug4140421.java
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.
 *
 * This code is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * version 2 for more details (a copy is included in the LICENSE file that
 * accompanied this code).
 *
 * You should have received a copy of the GNU General Public License version
 * 2 along with this work; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Please contact Oracle, 500 Oracle Parkway, Redwood Shores, CA 94065 USA
 * or visit www.oracle.com if you need additional information or have any
 * questions.
 *
 */

<<<<<<<< HEAD:test/hotspot/jtreg/runtime/interpreter/LastJsr.jasm
super public class LastJsr
{
    public static Method test:"()V"
    stack 100 locals 100
    {
        return;
    LABEL:
        nop;
        jsr LABEL; // bci=2. Compute bci + length(jsr) -> bci = 5 accessed, out of bounds.
========
/* @test
 * @bug 4140421
 * @summary Tests JToolBar set title correctly
 * @run main bug4140421
 */

import javax.swing.JToolBar;
import javax.swing.SwingUtilities;

public class bug4140421 {
    public static void main(String[] args) throws Exception {
        SwingUtilities.invokeAndWait(() -> {
            JToolBar tb = new JToolBar("MyToolBar");
            if (!tb.getName().equals("MyToolBar")) {
                throw new RuntimeException("Title of JToolBar set incorrectly...");
            }
        });
>>>>>>>> jdk-21.0.7+6:test/jdk/javax/swing/JToolBar/bug4140421.java
    }
}
