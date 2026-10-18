"""Compile production import/selection helpers and run structural game fixtures."""
from pathlib import Path
import subprocess,tempfile,os
root=Path(__file__).resolve().parents[2]
java=Path(os.environ['JAVA_HOME'])/'bin' if os.environ.get('JAVA_HOME') else None
sources=root/'android/app/src/main/java/com/dethrace/android'
with tempfile.TemporaryDirectory() as d:
 subprocess.run([str(java/'javac') if java else 'javac','-d',d,
  str(sources/'GameContent.java'),str(sources/'GameDataZip.java'),str(root/'test/android/GameDataZipTest.java'),str(root/'test/android/GameContentTest.java')],check=True)
 for test in ['GameDataZipTest','com.dethrace.android.GameContentTest']:
  subprocess.run([str(java/'java') if java else 'java','-cp',d,test],check=True)
