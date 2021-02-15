from setuptools import setup

setup(
  name='meanie3D',
  version='1.6.1',
  packages=["meanie3D", "meanie3D.app", "meanie3D.visualisation", "meanie3D.resources"],
  include_package_data=True,
  url='http://git.meteo.uni-bonn.de/projects/meanie3d',
  license='MIT License',
  author='Juergen Simon',
  author_email='simon@webtecc.com',
  zip_safe=False,
  description='Python scripts for running the meanie3D clustering and tracking algorithms and visualising the results.',
  entry_points={
    'console_scripts': [
      'meanie3D = meanie3D.meanie3D_run:main'
    ]
  }
)
