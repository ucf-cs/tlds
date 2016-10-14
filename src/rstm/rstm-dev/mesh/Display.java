/**
 *  Copyright (C) 2011
 *  University of Rochester Department of Computer Science
 *    and
 *  Lehigh University Department of Computer Science and Engineering
 *
 *  License: Modified BSD
 *           Please see the file LICENSE.RSTM for licensing information
 */

/*
    Display.java

    Graphical viewer for output of Delaunay triangularization.

    Author: Michael L. Scott, 2006-2007, based on traveling salesperson code
    originally written in 2002.
 */

import java.awt.*;
import java.awt.event.*;
import java.io.*;
import javax.swing.*;
import java.util.*;
import java.lang.*;
import java.lang.Thread.*;
import java.util.concurrent.*;
import java.util.Arrays;

public class Display extends JApplet {
    private static int pixels = 600;        // on each side
    private static int hesitation = 0;      // ms
    private static BufferedReader inp;      // stdin

    private static void parseArgs(String[] args) {
        for (int i = 0; i < args.length; i++) {
            String option = args[i];
            if (++i < args.length) {
                int arg = Integer.valueOf(args[i]);
                if (option.contentEquals("-p")) {
                    pixels = arg;
                } else if (option.contentEquals("-h")) {
                    hesitation = arg;
                }
            }
        }
    }

    private void buildUI(RootPaneContainer pane) {
        new UI(pane, pixels, hesitation, inp);
    }

    public static void main(String[] args) {
        parseArgs(args);
        inp = new BufferedReader(new InputStreamReader(System.in));
        try {
            String meshArgs = inp.readLine();
            JFrame f = new JFrame(meshArgs);
            f.addWindowListener(new WindowAdapter() {
                public void windowClosing(WindowEvent e) {
                    System.exit(0);
                }
            });
            Display me = new Display();
            me.buildUI(f);
            f.pack();
            f.setVisible(true);
        } catch (IOException e) {
            System.err.println(e);
        }
    }
}

// The Coordinator serves to slow down execution, so that behavior is
// visible on the screen, and to notify all running threads when the user
// wants them to die.
//
class Coordinator {
    private boolean running = true;
        // set to false when threads are supposed to pause.
    private final int hesitation;

    // Wait a bit before proceeding through gate.
    //
    public void hesitate() {
        try {
            Thread.sleep(hesitation);   // milliseconds
        } catch(InterruptedException e) {};
        synchronized(this) {
            while (!running) {
                try {
                    wait();
                } catch(InterruptedException e) {};
            }
        }
    }

    // Toggle running.  Resume paused threads if appropriate.
    //
    public synchronized void toggle() {
        running = !running;
        if (running) {
            notifyAll();
        }
    }

    public Coordinator(int h) {
        hesitation = h;
    }
}

class Surface extends JPanel {
    private static int width;            // canvas dimensions
    private static int height;
    private static final int dotsize = 6;
    private static final int border = dotsize;

    private final Coordinator c;
    private final UI u;
    private BufferedReader inp;

    private int minX;
    private int maxX;
    private int minY;
    private int maxY;

    // The next two routines figure out where to render the dot
    // for a point, given the size of the canvas and the spread
    // of x and y values among all points.
    //
    private int xPosition(int x) {
        return (int)
            (((double)x-(double)minX)*(double)width
                /((double)maxX-(double)minX))+border;
    }
    private int yPosition(int y) {
        return (int)
            (((double)maxY-(double)y)*(double)height
                /((double)maxY-(double)minY))+border;
    }

    Set<edge> edges;

    class edge {
        private int ax;
        private int ay;
        private int bx;
        private int by;

        // Override Object.hashCode and Object.equals.
        // This way two edges are equal (and hash to the same slot in
        // hashSet edges) if they have the same coordinates, even if they
        // are different objects.  NB: equals assumes that points are
        // sorted.
        //
        public int hashCode() {
            return ax ^ ay ^ bx ^ by;
        }
        public boolean equals(Object o) {
            edge e = (edge) o;              // run-time type check
            return ax == e.ax && ay == e.ay && bx == e.bx && by == e.by;
        }

        public edge(String s) {
            StringTokenizer st = new StringTokenizer(s);
            ax = Integer.parseInt(st.nextToken());
            ay = Integer.parseInt(st.nextToken());
            bx = Integer.parseInt(st.nextToken());
            by = Integer.parseInt(st.nextToken());
            repaint();
        }

        // Render self on the Surface canvas.
        //
        public void paint(Graphics2D g) {
            g.setPaint(Color.black);
            g.drawLine(xPosition(ax), yPosition(ay),
                       xPosition(bx), yPosition(by));
            g.setPaint(Color.blue);
            g.fillOval(xPosition(ax)-dotsize/2,
                       yPosition(ay)-dotsize/2,
                       dotsize, dotsize);
            g.fillOval(xPosition(bx)-dotsize/2,
                       yPosition(by)-dotsize/2,
                       dotsize, dotsize);
            repaint();
        }
    }

    // The following method is called automatically by the graphics
    // system when it thinks the Surface canvas needs to be
    // re-displayed.  This can happen because code elsewhere in this
    // program called repaint(), or because of hiding/revealing or
    // open/close operations in the surrounding window system.
    //
    public void paintComponent(Graphics g) {
        final Graphics2D g2 = (Graphics2D) g;

        super.paintComponent(g);
        // The following is synchronized to avoid race conditions with
        // worker thread.
        synchronized (edges) {
            for (edge e : edges) {
                e.paint(g2);
            }
        }
    }

    public class Worker extends Thread {
        public void run() {
            try {
                String s = inp.readLine();
                while (s != null) {
                    c.hesitate();
                    if (s.charAt(0) == 't') {
                        StringTokenizer st
                            = new StringTokenizer(s.substring(6));
                        u.updateTime(Long.parseLong(st.nextToken()));
                    } else if (s.charAt(0) == '+') {
                        edge e = new edge(s.substring(2));
                        synchronized (edges) {
                            edges.add(e);
                        }
                    } else if (s.charAt(0) == '-') {
                        edge e = new edge(s.substring(2));
                        synchronized (edges) {
                            edges.remove(e);
                        }
                    }
                    s = inp.readLine();
                }
            } catch (IOException e) {
                System.err.println(e);
            }
            // Tell the event thread to unset the default button when it
            // gets a chance.  (Threads other than the event thread
            // cannot safely modify the GUI directly.)
            SwingUtilities.invokeLater(new Runnable() {
                public void run() {
                    getRootPane().setDefaultButton(null);
                }
            });
        }
    }

    public Surface(Coordinator C, UI U, int pixels, BufferedReader stdin) {
        c = C;
        u = U;
        width = height = pixels;
        inp = stdin;

        setPreferredSize(new Dimension(width+border*2, height+border*2));
        setBackground(Color.white);
        setForeground(Color.black);

        try {
            StringTokenizer st = new StringTokenizer(inp.readLine());
            minX = Integer.parseInt(st.nextToken());
            maxX = Integer.parseInt(st.nextToken());
            minY = Integer.parseInt(st.nextToken());
            maxY = Integer.parseInt(st.nextToken());
        } catch (IOException e) {
            System.err.println(e);
        }
        edges = new HashSet<edge>();
    }

    public void triangulate() {
        new Worker().start();
    }
}

// Class UI is the user interface.  It displays a Surface canvas above
// a row of buttons and a row of statistics.  Actions (event handlers)
// are defined for each of the buttons.  Depending on the state of the
// UI, either the "run" or the "pause" button is the default (highlighted in
// most window systems); it will often self-push if you hit carriage return.
//
class UI extends JPanel {
    private final Coordinator c;
    private final Surface t;

    private final JRootPane root;
    private static final int externalBorder = 6;

    private static final int stopped = 0;
    private static final int running = 1;
    private static final int paused = 2;

    private int state = stopped;

    private final JLabel timeLabel = new JLabel("time: 0");

    public void updateTime(long elapsedTime) {
        timeLabel.setText("time: " + (double) elapsedTime/1000);
        repaint();
    }

    // Constructor
    //
    public UI(RootPaneContainer pane, int pixels,
              int hesitation, BufferedReader inp) {
        final UI u = this;
        c = new Coordinator(hesitation);
        t = new Surface(c, u, pixels, inp);

        final JPanel b = new JPanel();   // button panel

        final JButton runButton = new JButton("Run");
        final JButton pauseButton = new JButton("Pause");
        final JButton quitButton = new JButton("Quit");

        final JPanel s = new JPanel();   // statistics panel

        runButton.addActionListener(new ActionListener() {
            public void actionPerformed(ActionEvent e) {
                if (state == stopped) {
                    state = running;
                    root.setDefaultButton(pauseButton);
                    Date d = new Date();
                    t.triangulate();
                } else if (state == paused) {
                    state = running;
                    root.setDefaultButton(pauseButton);
                    Date d = new Date();
                    c.toggle();
                }
            }
        });
        pauseButton.addActionListener(new ActionListener() {
            public void actionPerformed(ActionEvent e) {
                if (state == running) {
                    state = paused;
                    root.setDefaultButton(runButton);
                    c.toggle();
                }
            }
        });
        quitButton.addActionListener(new ActionListener() {
            public void actionPerformed(ActionEvent e) {
                System.exit(0);
            }
        });

        // put the buttons into the button panel:
        b.setLayout(new FlowLayout());
        b.add(runButton);
        b.add(pauseButton);
        b.add(quitButton);

        // put the labels into the statistics panel:
        s.add(timeLabel);

        // put the Surface canvas, the button panel, and the stats
        // label into the UI:
        setLayout(new BoxLayout(this, BoxLayout.Y_AXIS));
        setBorder(BorderFactory.createEmptyBorder(
            externalBorder, externalBorder, externalBorder, externalBorder));
        add(t);
        add(b);
        add(s);

        // put the UI into the Frame or Applet:
        pane.getContentPane().add(this);
        root = getRootPane();
        root.setDefaultButton(runButton);
    }
}
