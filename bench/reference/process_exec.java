import java.io.*;
class process_exec {
    static String exec(String cmd) throws Exception {
        Process p = Runtime.getRuntime().exec(new String[]{"sh","-c",cmd});
        String out = new String(p.getInputStream().readAllBytes()).trim();
        p.waitFor(); return out;
    }
    public static void main(String[] args) throws Exception {
        String a = exec("echo hello"), b = exec("echo world"), c = exec("echo yona");
        System.out.println(a.length() + b.length() + c.length());
    }
}
