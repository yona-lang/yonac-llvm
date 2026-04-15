import Control.Concurrent (threadDelay)
import Control.Concurrent.Async (concurrently)
slow :: Int -> IO Int
slow ms = threadDelay (ms * 1000) >> return ms
main :: IO ()
main = do
  ((a, b), (c, d)) <- concurrently (concurrently (slow 100) (slow 100))
                                   (concurrently (slow 100) (slow 100))
  print (a + b + c + d)
