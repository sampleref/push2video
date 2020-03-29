package push2video.utils;

public class Push2VideoUtils {

    public static String generateRandomString() {
        return Integer.toString(getRandomNumber(12321, 99999));
    }

    private static int getRandomNumber(int min, int max) {
        return (int) ((Math.random() * (max - min)) + min);
    }

    public static String getEnv(String key, String defValue) {
        String value = System.getenv(key);
        if (value == null) {
            return defValue;
        }
        return value;
    }
}
