import android.content.Context;
import android.media.session.MediaController;
import android.media.session.MediaSessionManager;
import android.os.Looper;

public final class AdbMediaSeek {
    public static void main(String[] args) throws Exception {
        Looper.prepare();
        Class<?> at = Class.forName("android.app.ActivityThread");
        Object thread = at.getMethod("systemMain").invoke(null);
        Context context = (Context) at.getMethod("getSystemContext").invoke(thread);
        Class<?> serviceManager = Class.forName("android.media.MediaServiceManager");
        Class.forName("android.media.MediaFrameworkPlatformInitializer")
            .getMethod("setMediaServiceManager", serviceManager)
            .invoke(null, serviceManager.getConstructor().newInstance());
        MediaSessionManager manager = (MediaSessionManager) context.getSystemService(Context.MEDIA_SESSION_SERVICE);
        for (MediaController controller : manager.getActiveSessions(null)) {
            if (controller.getPackageName().equals(args[0])) {
                controller.getTransportControls().seekTo(Long.parseLong(args[1]));
                System.out.println("Seek requested: " + args[1]);
                System.exit(0);
            }
        }
        System.err.println("No active media session for " + args[0]);
        System.exit(1);
    }
}
